/*
 * uartlite.c: Serial driver for Xilinx uartlite serial controller
 *
 * Copyright (C) 2006 Peter Korsgaard <jacmet@sunsite.dk>
 * Copyright (C) 2007 Secret Lab Technologies Ltd.
 * Copyright (C) 2020 Chien-Wei Lan <chienwei@xilinx.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

#define ULITE_NAME		"ttyXRTUL"
#define ULITE_NR_UARTS		64

/* ---------------------------------------------------------------------
 * Register definitions
 *
 * For register details see datasheet:
 * http://www.xilinx.com/support/documentation/ip_documentation/opb_uartlite.pdf
 */

#define ULITE_RX		0x00
#define ULITE_TX		0x04
#define ULITE_STATUS		0x08
#define ULITE_CONTROL		0x0c

#define ULITE_REGION		16

#define ULITE_STATUS_RXVALID	0x01
#define ULITE_STATUS_RXFULL	0x02
#define ULITE_STATUS_TXEMPTY	0x04
#define ULITE_STATUS_TXFULL	0x08
#define ULITE_STATUS_IE		0x10
#define ULITE_STATUS_OVERRUN	0x20
#define ULITE_STATUS_FRAME	0x40
#define ULITE_STATUS_PARITY	0x80

#define ULITE_CONTROL_RST_TX	0x01
#define ULITE_CONTROL_RST_RX	0x02
#define ULITE_CONTROL_IE	0x10

struct uartlite_data {
	const struct uartlite_reg_ops	*reg_ops;
	struct uart_driver	*xcl_ulite_driver;
	struct uart_port	*port;
	atomic_t		console_opened;
	struct task_struct	*thread;
	struct mutex 		lock;
};

static struct uart_port ulite_ports[ULITE_NR_UARTS];

static ssize_t console_name_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct uart_port *port = platform_get_drvdata(to_platform_device(dev));
        struct uartlite_data *pdata = port->private_data;
        struct uart_driver *drv = pdata->xcl_ulite_driver;

	return sprintf(buf, "%s%d\n", drv->dev_name, port->line);
}

static DEVICE_ATTR_RO(console_name);

static struct attribute *ulite_attrs[] = {
	&dev_attr_console_name.attr,
	NULL,
};

static struct attribute_group ulite_attr_group = {
	.attrs = ulite_attrs,
};

struct uartlite_reg_ops {
	u32 (*in)(void __iomem *addr);
	void (*out)(u32 val, void __iomem *addr);
};

static u32 uartlite_inbe32(void __iomem *addr)
{
	return ioread32be(addr);
}

static void uartlite_outbe32(u32 val, void __iomem *addr)
{
	iowrite32be(val, addr);
}

static const struct uartlite_reg_ops uartlite_be = {
	.in = uartlite_inbe32,
	.out = uartlite_outbe32,
};

static u32 uartlite_inle32(void __iomem *addr)
{
	return ioread32(addr);
}

static void uartlite_outle32(u32 val, void __iomem *addr)
{
	iowrite32(val, addr);
}

static const struct uartlite_reg_ops uartlite_le = {
	.in = uartlite_inle32,
	.out = uartlite_outle32,
};

static inline u32 uart_in32(u32 offset, struct uart_port *port)
{
	struct uartlite_data *pdata = port->private_data;

	return pdata->reg_ops->in(port->membase + offset);
}

static inline void uart_out32(u32 val, u32 offset, struct uart_port *port)
{
	struct uartlite_data *pdata = port->private_data;

	pdata->reg_ops->out(val, port->membase + offset);
}
/* ---------------------------------------------------------------------
 * Core UART driver operations
 */

static int ulite_receive(struct uart_port *port, int stat)
{
	struct tty_port *tport = &port->state->port;
	unsigned char ch = 0;
	char flag = TTY_NORMAL;

	if ((stat & (ULITE_STATUS_RXVALID | ULITE_STATUS_OVERRUN
		     | ULITE_STATUS_FRAME)) == 0)
		return 0;

	/* stats */
	if (stat & ULITE_STATUS_RXVALID) {
		port->icount.rx++;
		ch = uart_in32(ULITE_RX, port);

		if (stat & ULITE_STATUS_PARITY)
			port->icount.parity++;
	}

	if (stat & ULITE_STATUS_OVERRUN)
		port->icount.overrun++;

	if (stat & ULITE_STATUS_FRAME)
		port->icount.frame++;


	/* drop byte with parity error if IGNPAR specificed */
	if (stat & port->ignore_status_mask & ULITE_STATUS_PARITY)
		stat &= ~ULITE_STATUS_RXVALID;

	stat &= port->read_status_mask;

	if (stat & ULITE_STATUS_PARITY)
		flag = TTY_PARITY;


	stat &= ~port->ignore_status_mask;

	if (stat & ULITE_STATUS_RXVALID)
		tty_insert_flip_char(tport, ch, flag);

	if (stat & ULITE_STATUS_FRAME)
		tty_insert_flip_char(tport, 0, TTY_FRAME);

	if (stat & ULITE_STATUS_OVERRUN)
		tty_insert_flip_char(tport, 0, TTY_OVERRUN);

	return 1;
}

/* commit 1788cf6a91d9 ("tty: serial: switch from circ_buf to kfifo") */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 10, 0)
static inline bool ulite_uart_is_empty(struct uart_port *p)
{
	return uart_circ_empty(&p->state->xmit);
}
static inline unsigned ulite_uart_pending(struct uart_port *p)
{
	return uart_circ_chars_pending(&p->state->xmit);
}
static inline int ulite_uart_pop_char(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	u8 ret = xmit->buf[xmit->tail];

	/*
	 * When the tail of the circular buffer is reached, the next
	 * byte is transferred to the beginning of the buffer.
	 */
	xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE-1);
	return ret;
}
#else
static inline bool ulite_uart_is_empty(struct uart_port *p)
{
	return kfifo_is_empty(&p->state->port.xmit_fifo);
}
static inline unsigned ulite_uart_pending(struct uart_port *p)
{
	return kfifo_len(&p->state->port.xmit_fifo);
}
static inline int ulite_uart_pop_char(struct uart_port *port)
{
	struct tty_port *tport = &port->state->port;
	u8 ret;

	if (!kfifo_get(&tport->xmit_fifo, &ret))
		return -1;
	return ret;
}
#endif

static int ulite_transmit(struct uart_port *port, int stat)
{
	int ch;

	if (stat & ULITE_STATUS_TXFULL)
		return 0;

	if (port->x_char) {
		uart_out32(port->x_char, ULITE_TX, port);
		port->x_char = 0;
		port->icount.tx++;
		return 1;
	}

	if (ulite_uart_is_empty(port) || uart_tx_stopped(port))
		return 0;

	ch = ulite_uart_pop_char(port);
	if (ch <= 0)
		return 0;
	uart_out32((char)ch, ULITE_TX, port);
	port->icount.tx++;

	/* wake up */
	if (ulite_uart_pending(port) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	return 1;
}

static void ulite_worker(struct uart_port *port)
{
	int stat, busy, n = 0;
	unsigned long flags;

	do {
		spin_lock_irqsave(&port->lock, flags);
		stat = uart_in32(ULITE_STATUS, port);
		busy  = ulite_receive(port, stat);
		busy |= ulite_transmit(port, stat);
		spin_unlock_irqrestore(&port->lock, flags);
		n++;
	} while (busy && !kthread_should_stop());

	/* work done? */
	if (n > 1)
		tty_flip_buffer_push(&port->state->port);

}

static int ulite_thread(void *data)
{
	struct uartlite_data *pdata = (struct uartlite_data *)data;
	struct uart_port *port = pdata->port;
	int ret = 0;

	while (atomic_read(&pdata->console_opened) && !kthread_should_stop()) {
		ulite_worker(port);
		/* 115200bps / 9bits * 2 sampling rate 
		 * 25600Hz, we should sleep less than 40us
		 */
		usleep_range(30, 40);
	}

	return ret;
}

static unsigned int ulite_tx_empty(struct uart_port *port)
{
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&port->lock, flags);
	ret = uart_in32(ULITE_STATUS, port);
	spin_unlock_irqrestore(&port->lock, flags);

	return ret & ULITE_STATUS_TXEMPTY ? TIOCSER_TEMT : 0;
}

static unsigned int ulite_get_mctrl(struct uart_port *port)
{
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void ulite_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	/* N/A */
}

static void ulite_stop_tx(struct uart_port *port)
{
	/* N/A */
}

static void ulite_start_tx(struct uart_port *port)
{
	ulite_transmit(port, uart_in32(ULITE_STATUS, port));
}

static void ulite_stop_rx(struct uart_port *port)
{
	/* don't forward any more data (like !CREAD) */
	port->ignore_status_mask = ULITE_STATUS_RXVALID | ULITE_STATUS_PARITY
		| ULITE_STATUS_FRAME | ULITE_STATUS_OVERRUN;
}

static void ulite_break_ctl(struct uart_port *port, int ctl)
{
	/* N/A */
}

static int ulite_startup(struct uart_port *port)
{
	struct uartlite_data *pdata = port->private_data;
	int ret = 0;

	mutex_lock(&pdata->lock);
	atomic_inc(&pdata->console_opened);
	pdata->thread = kthread_run(ulite_thread, pdata, "ulite_thread");

	if (IS_ERR(pdata->thread)) {
		dev_err(port->dev, "fail to create thread\n");
		atomic_dec_if_positive(&pdata->console_opened);
		ret = PTR_ERR(pdata->thread);
		pdata->thread = NULL;
		mutex_unlock(&pdata->lock);
		return ret;
	}

	uart_out32(ULITE_CONTROL_RST_RX | ULITE_CONTROL_RST_TX,
		ULITE_CONTROL, port);
	uart_out32(ULITE_CONTROL_IE, ULITE_CONTROL, port);

	mutex_unlock(&pdata->lock);
	return 0;
}

static void ulite_shutdown(struct uart_port *port)
{
	struct uartlite_data *pdata = port->private_data;

	mutex_lock(&pdata->lock);
	if (atomic_read(&pdata->console_opened)) {
		atomic_dec_if_positive(&pdata->console_opened);
		(void)kthread_stop(pdata->thread);
		pdata->thread = NULL;
	}

	uart_out32(0, ULITE_CONTROL, port);
	uart_in32(ULITE_CONTROL, port); /* dummy */

	mutex_unlock(&pdata->lock);
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 1, 0)) || defined(RHEL_9_2_GE)
static void ulite_set_termios(struct uart_port *port, struct ktermios *termios,
			      const struct ktermios *old)
#else
static void ulite_set_termios(struct uart_port *port, struct ktermios *termios,
			      struct ktermios *old)
#endif
{
	unsigned long flags;
	unsigned int baud;

	spin_lock_irqsave(&port->lock, flags);

	port->read_status_mask = ULITE_STATUS_RXVALID | ULITE_STATUS_OVERRUN
		| ULITE_STATUS_TXFULL;

	if (termios->c_iflag & INPCK)
		port->read_status_mask |=
			ULITE_STATUS_PARITY | ULITE_STATUS_FRAME;

	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= ULITE_STATUS_PARITY
			| ULITE_STATUS_FRAME | ULITE_STATUS_OVERRUN;

	/* ignore all characters if CREAD is not set */
	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |=
			ULITE_STATUS_RXVALID | ULITE_STATUS_PARITY
			| ULITE_STATUS_FRAME | ULITE_STATUS_OVERRUN;

	/* update timeout */
	baud = uart_get_baud_rate(port, termios, old, 0, 460800);
	uart_update_timeout(port, termios->c_cflag, baud);

	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *ulite_type(struct uart_port *port)
{
	return port->type == PORT_UARTLITE ? "uartlite" : NULL;
}

static void ulite_release_port(struct uart_port *port)
{
	release_mem_region(port->mapbase, ULITE_REGION);
	iounmap(port->membase);
	port->membase = NULL;
}

static int ulite_request_port(struct uart_port *port)
{
	struct uartlite_data *pdata = port->private_data;
	int ret;

	pr_debug("ulite console: port=%p; port->mapbase=%llx\n",
		 port, (unsigned long long) port->mapbase);

	if (!request_mem_region(port->mapbase, ULITE_REGION, "uartlite")) {
		dev_err(port->dev, "Memory region busy\n");
		return -EBUSY;
	}

	port->membase = ioremap(port->mapbase, ULITE_REGION);
	if (!port->membase) {
		dev_err(port->dev, "Unable to map registers\n");
		release_mem_region(port->mapbase, ULITE_REGION);
		return -EBUSY;
	}

	pdata->reg_ops = &uartlite_be;
	ret = uart_in32(ULITE_CONTROL, port);
	uart_out32(ULITE_CONTROL_RST_TX, ULITE_CONTROL, port);
	ret = uart_in32(ULITE_STATUS, port);
	/* Endianess detection */
	if ((ret & ULITE_STATUS_TXEMPTY) != ULITE_STATUS_TXEMPTY)
		pdata->reg_ops = &uartlite_le;

	return 0;
}

static void ulite_config_port(struct uart_port *port, int flags)
{
	if (!ulite_request_port(port))
		port->type = PORT_UARTLITE;
}

static int ulite_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* we don't want the core code to modify any port params */
	return -EINVAL;
}

static void ulite_pm(struct uart_port *port, unsigned int state,
		     unsigned int oldstate)
{
	/* N/A */
}

#ifdef CONFIG_CONSOLE_POLL
static int ulite_get_poll_char(struct uart_port *port)
{
	if (!(uart_in32(ULITE_STATUS, port) & ULITE_STATUS_RXVALID))
		return NO_POLL_CHAR;

	return uart_in32(ULITE_RX, port);
}

static void ulite_put_poll_char(struct uart_port *port, unsigned char ch)
{
	while (uart_in32(ULITE_STATUS, port) & ULITE_STATUS_TXFULL)
		cpu_relax();

	/* write char to device */
	uart_out32(ch, ULITE_TX, port);
}
#endif

static struct uart_ops ulite_ops = {
	.tx_empty	= ulite_tx_empty,
	.set_mctrl	= ulite_set_mctrl,
	.get_mctrl	= ulite_get_mctrl,
	.stop_tx	= ulite_stop_tx,
	.start_tx	= ulite_start_tx,
	.stop_rx	= ulite_stop_rx,
	.break_ctl	= ulite_break_ctl,
	.startup	= ulite_startup,
	.shutdown	= ulite_shutdown,
	.set_termios	= ulite_set_termios,
	.type		= ulite_type,
	.release_port	= ulite_release_port,
	.request_port	= ulite_request_port,
	.config_port	= ulite_config_port,
	.verify_port	= ulite_verify_port,
	.pm		= ulite_pm,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char	= ulite_get_poll_char,
	.poll_put_char	= ulite_put_poll_char,
#endif
};

static struct uart_driver xcl_ulite_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= XOCL_DEVNAME(XOCL_UARTLITE),
	.dev_name	= ULITE_NAME,
	.nr		= ULITE_NR_UARTS,
};

static int ulite_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct uartlite_data *pdata;
	struct uart_port *port;
	int irq = 0, ret = 0, id = 0;

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct uartlite_data),
			     GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	for (id = 0; id < ULITE_NR_UARTS; id++)
		if (ulite_ports[id].mapbase == 0)
			break;
	
	if (id >= ULITE_NR_UARTS) {
		dev_err(&pdev->dev, "%s%i too large\n", ULITE_NAME, id);
		ret = -EINVAL;
		goto done;
	}

	pdata->xcl_ulite_driver = &xcl_ulite_driver;

	pdata->port = &ulite_ports[id];
	port = pdata->port;

	spin_lock_init(&port->lock);
	port->fifosize = 16;
	port->regshift = 2;
	port->iotype = UPIO_MEM;
	port->iobase = 1; /* mark port in use */
	port->mapbase = res->start;
	port->membase = NULL;
	port->ops = &ulite_ops;
	port->irq = irq;
	port->flags = UPF_BOOT_AUTOCONF;
	port->dev = &pdev->dev;
	port->type = PORT_UNKNOWN;
	port->line = id;
	port->private_data = pdata;

	platform_set_drvdata(pdev, port);
	mutex_init(&pdata->lock);
	atomic_set(&pdata->console_opened, 0);

	ret = sysfs_create_group(&pdev->dev.kobj, &ulite_attr_group);
	if (ret) {
		xocl_err(&pdev->dev, "create ulite sysfs attrs failed: %d", ret);
		goto done;
	}
	/* Register the port */
	ret = uart_add_one_port(&xcl_ulite_driver, port);
	if (ret) {
		dev_err(&pdev->dev, "uart_add_one_port() failed; err=%i\n", ret);
		port->mapbase = 0;
		platform_set_drvdata(pdev, NULL);
		goto done;
	}

done:
	return ret;
}

/*
 * Sometime in Kernel version 6.5+ the platform_driver probe function will have
 * its return code changed from `int` to `void`. This is to enforce the notion
 * that the returned error code does nothing.
 * For now, older drivers should handle the error within this function and return
 * anything. Once the conversion of the return type is complete we must update
 * the return type from int to void and change all instances of `return 0` to
 * `return`.
 * https://elixir.bootlin.com/linux/latest/source/include/linux/platform_device.h#L211
 */
static int __ulite_remove(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);
	struct uartlite_data *pdata;

	if (!port)
		return 0;

	sysfs_remove_group(&pdev->dev.kobj, &ulite_attr_group);

	pdata = port->private_data;
	if (!pdata)
		return 0;

	atomic_set(&pdata->console_opened, 0);
	if (pdata->thread)
		kthread_stop(pdata->thread);

	uart_remove_one_port(pdata->xcl_ulite_driver, port);
	platform_set_drvdata(pdev, NULL);
	port->mapbase = 0;

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void ulite_remove(struct platform_device *pdev)
{
	__ulite_remove(pdev);
}
#else
#define ulite_remove __ulite_remove
#endif

struct xocl_drv_private ulite_priv = {
	.ops = NULL,
};

struct platform_device_id ulite_id_table[] = {
	{ XOCL_DEVNAME(XOCL_UARTLITE), (kernel_ulong_t)&ulite_priv },
	{ },
};

static struct platform_driver ulite_platform_driver = {
	.probe = ulite_probe,
	.remove = ulite_remove,
	.driver = {
		.name  = XOCL_DEVNAME(XOCL_UARTLITE),
	},
	.id_table = ulite_id_table,
};

/* ---------------------------------------------------------------------
 * Module setup/teardown
 */

int __init xocl_init_ulite(void)
{
	int ret;

	ret = uart_register_driver(&xcl_ulite_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&ulite_platform_driver);
	if (ret)
		uart_unregister_driver(&xcl_ulite_driver);

	return ret;
}

void xocl_fini_ulite(void)
{
	platform_driver_unregister(&ulite_platform_driver);
	uart_unregister_driver(&xcl_ulite_driver);
}
