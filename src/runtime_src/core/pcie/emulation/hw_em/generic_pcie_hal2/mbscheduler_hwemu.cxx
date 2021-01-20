#include "shim.h"
#include <chrono>
#include <cmath>
#include <algorithm>

#define	CU_ADDR_HANDSHAKE_MASK	(0xff)
#define	CU_ADDR_VALID(addr)	(((addr) | CU_ADDR_HANDSHAKE_MASK) != (uint64_t)-1)
#define ERT_P2P_CMDQ_ADDR 0x8000000000

#define csr_read32(base, r_off)			\
	ioread32((base) + (r_off) - ERT_CSR_ADDR)
#define csr_write32(val, base, r_off)		\
	iowrite32((val), (base) + (r_off) - ERT_CSR_ADDR)

# define SCHED_DEBUGF(format, ...) 
//# define SCHED_DEBUGF(format, ...) printf(format, ##__VA_ARGS__)
# define SCHED_INFO(format, ...) printf(format, ##__VA_ARGS__)

using namespace xclhwemhal2;

namespace hwemu {

    const uint32_t no_index = std::numeric_limits<uint32_t>::max();

    //! initialize static variable, which gives unique ID for each cmd
    uint64_t xocl_cmd::next_uid = 0;

    /*
     * xocl_cmd() - Initialize xocl_cmd
     *
     * Return: 
     */
    xocl_cmd::xocl_cmd()
    {
        bo = NULL;

        uid = ++next_uid; //! Assign unique ID for each new command
        cu_idx = -1;
        slot_idx = -1;
        cu_bitmap.reset();
        ert_pkt = NULL;
        state = ERT_CMD_STATE_NEW;
        aborted = false;
    }

    /*
     * opcode() - Command opcode
     * Return: Opcode per command packet
     */
    uint32_t xocl_cmd::opcode()
    {
        return ert_pkt->opcode;
    }

    /*
     * type() - Command type
     * Return: Type of command
     */
    uint32_t xocl_cmd::type()
    {
        return ert_pkt->type;
    }


    /*
     * cmd_uid() - Get unique id of command
     */
    uint64_t xocl_cmd::cmd_uid()
    {
        return uid;
    }

    /**
     * payload_size() - Command payload size
     * Return: Size in number of words of command packet payload
     */
    uint32_t xocl_cmd::payload_size()
    {
        return ert_pkt->count;
    }

    /**
     * packet_size() - Command packet size
     * Return: Size in number of uint32_t of command packet
     */
    uint32_t xocl_cmd::packet_size()
    {
        return payload_size() +
            sizeof(ert_pkt->header) / sizeof(uint32_t);
    }

    /**
     * cu_masks() - Number of command packet cu_masks
     * Return: Total number of CU masks in command packet
     */
    uint32_t xocl_cmd::num_cumasks()
    {
        return 1 + ert_cu->extra_cu_masks;
    }

    /**
     * regmap_size() - Size of regmap is payload size (n) minus the number of cu_masks
     * Return: Size of register map in number of words
     */
    uint32_t xocl_cmd::regmap_size()
    {
        return payload_size() - num_cumasks();
    }

    /*
     * packet()- ert packet
     * Return: returns the ert packet
     */
    ert_packet* xocl_cmd::packet()
    {
        return ert_pkt;
    }

    /*
     * regmap(): ert command regmap address
     * Return: returns the regmap address
     */
    uint32_t* xocl_cmd::regmap()
    {
        return ert_cu->data + ert_cu->extra_cu_masks;
    }


    /**
     * set_int_state() - Set internal command state used by scheduler only
     * @state: new command state per ert.h
     */
    void xocl_cmd::set_int_state(enum ert_cmd_state state)
    {
        this->state = state;
        SCHED_DEBUGF("-> %s(%lu,%d)\n", __func__, uid, state);
    }

    /**
     * set_state() - Set both internal and external state of a command
     *
     * The state is reflected externally through the command packet
     * as well as being captured in internal state variable
     *
     * @state: new state
     */
    void xocl_cmd::set_state(enum ert_cmd_state state)
    {
        this->state = state;
        ert_pkt->state = state;
        SCHED_DEBUGF("-> %s(%lu,%d)\n", __func__, uid, state);
    }


    /*
     * bo_init() - Initialize a command object with an exec BO
     *
     * In penguin mode, the command object caches the CUs available
     * to execute the command.  When ERT is enabled, the CU info
     * is not used.
     */
    void xocl_cmd::bo_init(xclemulation::drm_xocl_bo *bo)
    {
        SCHED_DEBUGF("-> %s(%lu)\n", __func__, uid);
        this->bo = bo;
        this->ert_pkt = (struct ert_packet *)bo->buf;

        // copy pkt cus to command object cu bitmap
        //if (type() == ERT_CU) 
        {
            uint32_t cumasks[4] = {0};

            cumasks[0] = ert_cu->cu_mask;
            for (uint32_t i = 0; i < num_cumasks() -1 ; ++i) {
                cumasks[i+1] = ert_cu->data[i];
            }

            //! Set cu_bitmap from cumasks
            for (int i =  num_cumasks(); i >= 0;  --i) {
                cu_bitmap <<= 32;
                cu_bitmap |= cumasks[i];
            }
        }
        SCHED_DEBUGF("<- %s\n", __func__);
    }
    /**
     * cmd_has_cu() - Check if this command object can execute on CU
     *
     * @cuidx: the index of the CU.	 Note that CU indicies start from 0.
     */
    bool xocl_cmd::has_cu(uint32_t cuidx)
    {
        return cu_bitmap.test(cuidx);
    }


    /**
     * first_cu() - Get index of first CU this command can use
     */
    uint32_t xocl_cmd::first_cu()
    {
        uint32_t cu_idx = MAX_CUS;
        for (uint32_t i =  0; i < MAX_CUS;  ++i) {
            if(cu_bitmap.test(i)) {
                cu_idx = i; break;
            }
        }
        return cu_idx;
    }

    /**
     * next_cu() - Get index of CU after @prev this command can use
     *
     * @prev: index of previous CU
     */
    uint32_t xocl_cmd::next_cu(uint32_t prev)
    {
        uint32_t cu_idx = MAX_CUS;
        for (uint32_t i =  prev + 1; i < MAX_CUS;  ++i) {
            if(cu_bitmap.test(i)) {
                cu_idx = i; break;
            }
        }
        return cu_idx;
    }

    /**
     * set_cu() - Lock command to one specific CU
     *
     * @cuidx: Selected specific CU that this command can use
     */
    void xocl_cmd::set_cu(uint32_t cuidx)
    {
        this->cu_idx = cuidx;
        cu_bitmap.reset();
        cu_bitmap.set(cuidx);
    }


    /*****************************************************
     * xocl_cu member functions
     *****************************************************/

    xocl_cu::xocl_cu(HwEmShim* dev)
    {
        this->xdevice = dev;
        this->error = false;
        this->idx = 0;
        this->uid = 0;
        this->control = 0;
        this->dataflow = 0;
        this->base = 0;
        this->addr = 0;
        this->polladdr = 0;
        this->ap_check = 0;
        this->ctrlreg = 0;
        this->done_cnt = 0;
        this->run_cnt = 0;
    }

    void xocl_cu::cu_init(unsigned int idx, uint64_t base, uint64_t addr, uint64_t polladdr)
    {
        this->error = false;
        this->idx = idx;
        this->control = (addr & 0x7); // bits [2-0]
        this->dataflow = (addr & 0x7) == AP_CTRL_CHAIN;
        this->base = base;
        this->addr = addr & ~0xff;  // clear encoded handshake and context
        this->polladdr = polladdr;
        this->ap_check = (control == AP_CTRL_CHAIN) ? (AP_DONE) : (AP_DONE | AP_IDLE);
        this->ctrlreg = 0;
        this->done_cnt = 0;
        this->run_cnt = 0;
    }


    uint64_t xocl_cu::cu_base_addr()
    {
        return base + addr;
    }

    uint64_t xocl_cu::cu_polladdr()
    {
        return base + polladdr;
    }

    bool xocl_cu::cu_dataflow()
    {
        return (control == AP_CTRL_CHAIN);
    }

    bool xocl_cu::cu_valid()
    {
        return CU_ADDR_VALID(this->addr);
    }

    /**
     * cu_continue() - Acknowledge AP_DONE by sending AP_CONTINUE
     *
     * Applicable to dataflow only.
     *
     * In ERT poll mode, also write to the CQ slot corresponding to the CU.  ERT
     * prevents host notification of next AP_DONE until first AP_DONE is
     * acknowledged by host.  Do not acknowledge ERT if no outstanding jobs on CU;
     * this prevents stray notifications from ERT.
     */
    void xocl_cu::cu_continue()
    {
        if (!cu_dataflow())
            return;

        // acknowledge done directly to CU (xcu->addr)
        iowrite32(AP_CONTINUE, cu_base_addr());

        // in ert_poll mode acknowlegde done to ERT
        if (polladdr && run_cnt) 
        {
            iowrite32(AP_CONTINUE, cu_polladdr());
        }
    }

    uint32_t xocl_cu::cu_status()
    {
        return ioread32(cu_base_addr());
    }


    /**
     * cu_poll() - Poll a CU for its status
     *
     * Used in penguin and ert_poll mode only. Read the CU control register and
     * update run and done count as necessary.  Acknowledge any AP_DONE received
     * from kernel.  Check for AP_IDLE since ERT in poll mode will also read the
     * kernel control register and AP_DONE is COR.
     */
    void xocl_cu::cu_poll()
    {
        xdevice->xclRead(XCL_ADDR_KERNEL_CTRL, cu_base_addr(),(void*)&(ctrlreg),4);
        if (run_cnt && (ctrlreg & ap_check)) {
            ++done_cnt;
            --run_cnt;
            cu_continue();
        }
    }

    /**
     * cu_ready() - Check if CU is ready to start another command
     *
     * Return: True if ready false otherwise.
     * The CU is ready when AP_START is low.  Poll the CU if necessary.
     */
    bool xocl_cu::cu_ready()
    {
        if ((ctrlreg & AP_START) || (!cu_dataflow() && run_cnt))
            cu_poll();

        return cu_dataflow() ? !(ctrlreg & AP_START) : run_cnt == 0;
    }

    /**
     * cu_first_done() - Get the first completed command from the running queue
     *
     * Return: The first command that has completed or nullptr if none
     */
    xocl_cmd* xocl_cu::cu_first_done()
    {
        if (!done_cnt && run_cnt)
            cu_poll();

        return done_cnt ? cu_cmdq.front() : NULL;
    }

    /**
     * cu_pop_done() - Remove first element from running queue
     */
    void xocl_cu::cu_pop_done()
    {
        if (!done_cnt)
            return;

        cu_cmdq.pop();
        --done_cnt;
    }

    /**
     * cu_configure_ooo() - Configure a CU with {addr,val} pairs (out-of-order)
     */
    void  xocl_cu::cu_configure_ooo(xocl_cmd *xcmd)
    {
        unsigned int size = xcmd->regmap_size();
        uint32_t *regmap =  xcmd->regmap();
        unsigned int idx;

        // past reserved 4 ctrl + 2 ctx
        for (idx = 6; idx < size - 1; idx += 2) {
            uint32_t offset = *(regmap + idx);
            uint32_t val = *(regmap + idx + 1);

            iowrite32(val, cu_base_addr() + offset);
        }
    }

    /**
     * cu_configure_ino() - Configure a CU with consecutive layout (in-order)
     */
    void xocl_cu::cu_configure_ino(xocl_cmd *xcmd)
    {
        unsigned int size = xcmd->regmap_size();
        uint32_t *regmap =  xcmd->regmap();
        unsigned int idx;

        for (idx = 4; idx < size; ++idx) {
            iowrite32(*(regmap + idx), cu_base_addr() + (idx << 2));
        }
    }

    /**
     * cu_start() - Start the CU with a new command.
     *
     * The command is pushed onto the running queue
     */
    bool xocl_cu::cu_start(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s cu(%d) cmd(%lu)\n", __func__, idx, xcmd->uid);

        // write register map, starting at base + 0x10
        // 0x0 used for control register
        // 0x4, 0x8 used for interrupt, which is initialized in setup of ERT
        // 0xC used for interrupt status, which is set by hardware
        if (xcmd->opcode() == ERT_EXEC_WRITE)
            cu_configure_ooo(xcmd);
        else
            cu_configure_ino(xcmd);

        // start cu.  update local state as we may not be polling prior
        // to next ready check.
        ctrlreg |= AP_START;
        iowrite32(AP_START, this->cu_base_addr());

        // in ert poll mode request ERT to poll CU
        if (polladdr) {
            SCHED_DEBUGF("polladdr  @0x%lx\n", this->cu_polladdr());
            iowrite32(AP_START, polladdr);
        }

        cu_cmdq.push(xcmd);
        ++run_cnt;
        	
        SCHED_DEBUGF("<- %s cu(%d) started xcmd(%lu) done(%d) run(%d) ctrlreg(%d)\n",
		     __func__, idx, xcmd->uid, done_cnt, run_cnt, ctrlreg);

        return true;
    }

    void xocl_cu::iowrite32(uint32_t data, uint64_t addr)
    {
        if (addr >= ERT_P2P_CMDQ_ADDR) {
            xdevice->xclCopyBufferHost2Device(addr, (void*)(&data) ,4,0,XCL_ADDR_SPACE_DEVICE_RAM);
        } else {
            xdevice->xclWrite(XCL_ADDR_KERNEL_CTRL, addr, (void*)(&data) ,4);
        }
    }

    uint32_t  xocl_cu::ioread32(uint64_t addr) 
    {
        uint32_t data = 0;
        xdevice->xclRead(XCL_ADDR_KERNEL_CTRL, addr, (void*)&data, 4);
        return data;
    }

    void xocl_cu::xocl_memcpy_toio(uint64_t addr, uint32_t* data, uint32_t len) 
    {
        xdevice->xclCopyBufferHost2Device(addr, (void*)(&data), len ,0,XCL_ADDR_SPACE_DEVICE_RAM);
    }

    void xocl_cu::xocl_memcpy_fromio(uint32_t* data, uint64_t addr, uint32_t len) 
    {
        xdevice->xclCopyBufferDevice2Host((void*)(&data),  addr, len, 0, XCL_ADDR_SPACE_DEVICE_RAM);
    }

    /*****************  xocl ert *******************************/
    /**********************************************************/

    xocl_ert::xocl_ert(HwEmShim* dev, uint64_t csr_base, uint64_t cq_base)
    {
        this->xdevice = dev;
        this->csr_base = csr_base;
        this->cq_base = cq_base;
        this->num_slots = 0;
        this->slot_size = 0;
        this->cq_intr = false;
        this->uid = 0;
        this->cq_size = 0;
        this->ctrl_busy = false;
        this->version = 0;
        uint32_t idx;
        for (idx = 0; idx < MAX_SLOTS; ++idx) {
            this->command_queue[idx] = NULL;
        }
    }

    xocl_ert::~xocl_ert()
    {
    }

    /**
     * configure ert with cq size num_slots
     */
    void xocl_ert::ert_cfg(uint32_t cq_size, uint32_t num_slots, bool cq_intr)
    {
        uint32_t idx;

        SCHED_DEBUGF("%s cq_size(0x%x) slots(%d) slot_size(0x%x) cq_intr(%d)\n",
                __func__,  cq_size, num_slots, cq_size / num_slots, cq_intr);
        this->cq_size = cq_size;
        this->num_slots = num_slots;
        this->slot_size = cq_size / num_slots;
        this->cq_intr = cq_intr;
        this->version = 0;

        for (idx = 0; idx < MAX_CUS; ++idx) {
            this->cu_usage[idx] = 0;
            this->cu_status[idx] = 0;
        }

        for (idx = 0; idx < MAX_SLOTS; ++idx) {
            this->command_queue[idx] = NULL;
            this->cq_slot_status[idx] = 0;
            this->cq_slot_usage[idx] = 0;
        }

        this->slot_status.reset();
        this->slot_status.set(0); // reserve for control command
        this->ctrl_busy = false;
    }

    /**
     * Clear the ERT command queue status register
     *
     * This can be necessary in ert polling mode, where KDS itself
     * can be ahead of ERT, so stale interrupts are possible which
     * is bad during reconfig.
     */
    void xocl_ert::ert_clear_csr()
    {
        uint32_t idx;
        for (idx = 0; idx < 4; ++idx) 
        {
            //! Read to clear the status register
            uint32_t csr_addr = this->csr_base + (idx<<2);
            ioread32(csr_addr);
        }
    }

    /*
     * acquire_slot_idx() - First available slot index
     */
    uint32_t xocl_ert::ert_acquire_slot_idx()
    {
        uint32_t idx = no_index;
        for(uint32_t i = 0; i < MAX_CUS; i++) {
            if(!this->slot_status.test(i)) {
                idx = i; break;
            }
        }

        SCHED_DEBUGF("%s(%d) returns %d\n", __func__, this->uid, idx < this->num_slots ? idx : no_index);
        if (idx < this->num_slots) {
            this->slot_status.set(idx);
            return idx;
        }
        return no_index;
    }


    /**
     * acquire_slot() - Acquire a slot index for a command
     *
     * This function makes a special case for control commands which
     * must always dispatch to slot 0, otherwise normal acquisition
     */
    uint32_t xocl_ert::ert_acquire_slot(xocl_cmd *xcmd)
    {
        // slot 0 is reserved for ctrl commands
        if (xcmd->type() == ERT_CTRL) {
            SCHED_DEBUGF("%s ctrl cmd(%lu)\n", __func__, xcmd->uid);
            if (this->ctrl_busy) {
                SCHED_INFO("ctrl slot is busy\n");
                return -1;
            }
            this->ctrl_busy = true;
            return (xcmd->slot_idx = 0);
        }

        return (xcmd->slot_idx = ert_acquire_slot_idx());
    }

    /*
     * release_slot_idx() - Release specified slot idx
     */
    void xocl_ert::ert_release_slot_idx(uint32_t slot_idx)
    {
        this->slot_status.reset(slot_idx);
    }

    /**
     * release_slot() - Release a slot index for a command
     *
     * Special case for control commands that execute in slot 0.  This
     * slot cannot be marked free ever.
     */
    void xocl_ert::ert_release_slot(xocl_cmd *xcmd)
    {
        if (xcmd->slot_idx == no_index)
            return; // already released

        SCHED_DEBUGF("-> %s(%d) xcmd(%lu) slotidx(%d)\n",
                __func__, this->uid, xcmd->uid, xcmd->slot_idx);
        if (xcmd->type() == ERT_CTRL) {
            SCHED_DEBUGF("+ ctrl cmd\n");
            this->ctrl_busy = false;
        } else {
            ert_release_slot_idx(xcmd->slot_idx);
        }
        this->command_queue[xcmd->slot_idx] = NULL;
        xcmd->slot_idx = no_index;
        SCHED_DEBUGF("<- %s\n", __func__);
    }

    xocl_cmd* xocl_ert::ert_get_cmd(uint32_t slotidx)
    {
        return this->command_queue[slotidx];
    }

    /**
     * ert_start_cmd() - Start a command in ERT mode
     *
     * @xcmd: command to start
     *
     * Write command packet to ERT command queue
     */
    bool xocl_ert::ert_start_cmd(xocl_cmd *xcmd)
    {
        uint32_t slot_addr = 0;
        ert_packet *ecmd = xcmd->packet();

        SCHED_DEBUGF("-> %s ert(%d) cmd(%lu)\n", __func__, this->uid, xcmd->uid);

        if (ert_acquire_slot(xcmd) == no_index) {
            SCHED_DEBUGF("<- %s returns false (noindex)\n", __func__);
            return false;
        }

        slot_addr = xcmd->slot_idx * this->slot_size;

        //SCHED_DEBUG_PACKET(ecmd, cmd_packet_size(xcmd));

        // write packet minus header
        if (!xdevice->isLegacyErt() && xcmd->type() == ERT_CU) {
            // write kds selected cu_idx in first cumask (first word after header)
            iowrite32(xcmd->cu_idx, this->cq_base + slot_addr + 4);

            // write remaining packet (past header and cuidx)
            xocl_memcpy_toio(this->cq_base + slot_addr + 8,
                    ecmd->data + 1, (ecmd->count - 1) * sizeof(uint32_t));
        }
        else
            xocl_memcpy_toio(this->cq_base + slot_addr + 4,
                    ecmd->data, ecmd->count * sizeof(uint32_t));

        // write header
        iowrite32(ecmd->header, this->cq_base + slot_addr);

        // trigger interrupt to embedded scheduler if feature is enabled
        if (this->cq_intr) {
            uint32_t mask_idx = xcmd->slot_idx >> 5;
            uint32_t cq_int_addr = ERT_CQ_STATUS_REGISTER_ADDR + (mask_idx << 2);
            uint32_t mask = 1 << (xcmd->slot_idx & 0x1f);

            SCHED_DEBUGF("++ mb_submit writes slot mask 0x%x to CQ_INT register at addr 0x%x\n",
                    mask, cq_int_addr);
            csr_write32(mask, this->csr_base, cq_int_addr);
        }

        // success
        ++this->cq_slot_usage[xcmd->slot_idx];
        this->command_queue[xcmd->slot_idx] = xcmd;

        SCHED_DEBUGF("<- %s returns true\n", __func__);
        return true;
    }

    /**
     * New ERT populates:
     * [1  ]      : header
     * [1  ]      : custat version
     * [1  ]      : ert git version
     * [1  ]      : number of cq slots
     * [1  ]      : number of cus
     * [#numcus]  : cu execution stats (number of executions)
     * [#numcus]  : cu status (1: running, 0: idle)
     * [#slots]   : command queue slot status
     *
     * Old ERT populates
     * [1  ]      : header
     * [#numcus]  : cu execution stats (number of executions)
     */
    void xocl_ert::ert_read_custat(xocl_cmd *xcmd, uint32_t num_cus)
    {
        uint32_t slot_addr = xcmd->slot_idx*this->slot_size;

        // cu stat version is 1 word past header
        uint32_t custat_version = ioread32(this->cq_base + slot_addr + 4);

        this->version = -1;
        memset(this->cu_usage, -1, MAX_CUS * sizeof(uint32_t));
        memset(this->cu_status, -1, MAX_CUS * sizeof(uint32_t));
        memset(this->cq_slot_status, -1, MAX_SLOTS * sizeof(uint32_t));

        // New command style from ERT firmware
        if (custat_version == 0x51a10000) {
            uint32_t idx = 2; // packet word index past header and version
            uint32_t max_idx = (this->slot_size >> 2);
            uint32_t git = ioread32(this->cq_base + slot_addr + (idx++ << 2));
            uint32_t ert_num_cq_slots = ioread32(this->cq_base + slot_addr + (idx++ << 2));
            uint32_t ert_num_cus = ioread32(this->cq_base + slot_addr + (idx++ << 2));
            uint32_t words = 0;

            this->version = git;

            // bogus data in command, avoid oob writes to local arrays
            if (ert_num_cus > MAX_CUS || ert_num_cq_slots > MAX_CUS)
                return;

            // cu execution stat
            words = std::min(ert_num_cus, max_idx - idx);
            xocl_memcpy_fromio(this->cu_usage, this->cq_base + slot_addr + (idx << 2),
                    words * sizeof(uint32_t));
            idx += words;

            // ert cu status
            words = std::min(ert_num_cus, max_idx - idx);
            xocl_memcpy_fromio(this->cu_status, this->cq_base + slot_addr + (idx << 2),
                    words * sizeof(uint32_t));
            idx += words;

            // ert cq status
            words = std::min(ert_num_cq_slots, max_idx - idx);
            xocl_memcpy_fromio(this->cq_slot_status, this->cq_base + slot_addr + (idx << 2),
                    words * sizeof(uint32_t));
            idx += words;
        }
        else {
            // Old ERT command style populates only cu usage past header
            xocl_memcpy_fromio(this->cu_usage, this->cq_base + slot_addr + 4,
                    num_cus * sizeof(uint32_t));
        }
    }

    uint32_t xocl_ert::ert_version()
    {
        return this->version;
    }

    uint32_t xocl_ert::ert_cu_usage(uint32_t cuidx)
    {
        return this->cu_usage[cuidx];
    }

    uint32_t xocl_ert::ert_cu_status(uint32_t cuidx)
    {
        return this->cu_status[cuidx];
    }

    bool xocl_ert::ert_cq_slot_busy(uint32_t slotidx)
    {
        return this->command_queue[slotidx] != NULL;
    }

    uint32_t xocl_ert::ert_cq_slot_status(uint32_t slotidx)
    {
        return this->cq_slot_status[slotidx];
    }

    uint32_t xocl_ert::ert_cq_slot_usage(uint32_t slotidx)
    {
        return this->cq_slot_usage[slotidx];
    }

    void xocl_ert::iowrite32(uint32_t data, uint64_t addr)
    {
        SCHED_DEBUGF("-> %s addr(0x%lx) data(0x%x)\n", __func__, addr, data);

        if (addr >= ERT_P2P_CMDQ_ADDR) {
            xdevice->xclCopyBufferHost2Device(addr, (void*)(&data) ,4,0,XCL_ADDR_SPACE_DEVICE_RAM);
        } else {
            xdevice->xclWrite(XCL_ADDR_KERNEL_CTRL, addr, (void*)(&data) ,4);
        }
    }

    uint32_t  xocl_ert::ioread32(uint64_t addr) 
    {
        SCHED_DEBUGF("-> %s addr(0x%lx) \n", __func__, addr);
        uint32_t data = 0;
        xdevice->xclRead(XCL_ADDR_KERNEL_CTRL, addr, (void*)&data, 4);
        return data;
    }

    void xocl_ert::xocl_memcpy_toio(uint64_t addr, void* data, uint32_t len) 
    {
        SCHED_DEBUGF("-> %s addr(0x%lx) len(%d)\n", __func__, addr, len);

        if (addr >= ERT_P2P_CMDQ_ADDR) {
            xdevice->xclCopyBufferHost2Device(addr, (void*)(data), len ,0,XCL_ADDR_SPACE_DEVICE_RAM);
        } else {
            xdevice->xclWrite(XCL_ADDR_KERNEL_CTRL, addr, data  ,len);
        }
    }

    void xocl_ert::xocl_memcpy_fromio(void* data, uint64_t addr, uint32_t len) 
    {
        SCHED_DEBUGF("-> %s addr(0x%lx) len(%d)\n", __func__, addr, len);
        xdevice->xclCopyBufferDevice2Host((void*)(data),  addr, len, 0, XCL_ADDR_SPACE_DEVICE_RAM);
    }

    // exec_core io read/write
    void exec_core::iowrite32(uint32_t data, uint64_t addr)
    {
        SCHED_DEBUGF("-> %s addr(0x%lx) data(0x%x)\n", __func__, addr, data);

        if (addr >= ERT_P2P_CMDQ_ADDR) {
            xdevice->xclCopyBufferHost2Device(addr, (void*)(&data) ,4,0,XCL_ADDR_SPACE_DEVICE_RAM);
        } else {
            xdevice->xclWrite(XCL_ADDR_KERNEL_CTRL, addr, (void*)(&data) ,4);
        }
    }

    uint32_t  exec_core::ioread32(uint64_t addr) 
    {
        SCHED_DEBUGF("-> %s addr(0x%lx) \n", __func__, addr);

        uint32_t data = 0;
        xdevice->xclRead(XCL_ADDR_KERNEL_CTRL, addr, (void*)&data, 4);
        return data;
    }


    /******************************* exec_core ***********************************************/

    exec_core::exec_core(HwEmShim* dev, xocl_scheduler* sched)
    {
        static uint32_t next_uid = 1;
        uid     = next_uid++;

        xdevice   = dev;
        scheduler = sched;
        ert       = nullptr;

        base = 0;  //! Bar address
        csr_base  = 0;
        intr_base = 0;
        intr_num  = 0;

        cq_base = 0;
        cq_size = ERT_CQ_SIZE;

        num_cus = 0;
        num_cdma = 0;

        polling_mode = true;
        cq_interrupt = false;
        configure_active = false;
        configured = false;
        stopped = false;
        flush = false;
        ops = nullptr; 

        this->num_running_cmds = 0;
        this->num_pending_cmds = 0;

        for (unsigned int i=0; i<MAX_CUS; ++i) {
            cus[i] = nullptr;
            cu_usage[i] = 0;
            cu_status[i] = 0;
            cu_load_count[i] = 0;
        }

        sr0 = 0;
        sr1 = 0;
        sr2 = 0;
        sr3 = 0;

        uint32_t ert_version = atoi(xdevice->getERTVersion().c_str());
        if(ert_version>=30) {
            this->cq_base =  ERT_P2P_CMDQ_ADDR;       
            this->csr_base = 0x10000;
        } else {
            this->cq_base =  0x190000;
            this->csr_base = 0x180000;
        }
    }

    int exec_core::exec_cfg_cmd(xocl_cmd *xcmd)
    {
        ert_configure_cmd *cfg = xcmd->ert_cfg;

        int  ert_version = atoi(xdevice->getERTVersion().c_str());
        bool is_ert = xdevice->isMBSchedulerEnabled();
        
        //cfg->dataflow = false;
        bool ert_full = (is_ert && cfg->ert && !cfg->dataflow);
        bool ert_poll = (is_ert && cfg->ert && cfg->dataflow);

        // Only allow configuration with one live ctx
        if (configured) {
            SCHED_DEBUGF("command scheduler is already configured for this device\n");
            return 1;
        }

        if (ert_version > 30) {
            SCHED_INFO("Unknown ERT version, fallback to KDS mode\n");
            ert_full = 0;
            ert_poll = 0;
        }

        // Mark command as control command to force slot 0 execution
        cfg->type = ERT_CTRL;

        uint32_t ert_num_slots = 0;
        ert_num_slots = ERT_CQ_SIZE / cfg->slot_size;
        num_cdma = 0;

        if (ert_poll)
            // Adjust slot size for ert poll mode
            cfg->slot_size = this->cq_size / MAX_CUS;

        if (ert_full && cfg->cu_dma && ert_num_slots > 32) {
            // Max slot size is 32 because of cudma bug
            SCHED_DEBUGF("Limitting CQ size to 32 due to ERT CUDMA bug\n");
            ert_num_slots = 32;
            cfg->slot_size = this->cq_size / ert_num_slots;
        }

        // Create CUs for regular CUs
        uint32_t cuidx = 0;
        for (cuidx = 0; cuidx < cfg->num_cus; ++cuidx) 
        {
            xocl_cu *xcu = this->cus[cuidx];
            uint64_t polladdr = (ert_poll) 			
                // cuidx+1 to reserve slot 0 for ctrl => max 127 CUs in ert_poll mode
                ? this->cq_base + (cuidx+1) * cfg->slot_size : 0;

            if (!xcu)
                xcu = this->cus[cuidx] = new xocl_cu(xdevice);
            xcu->cu_init(cuidx, this->base, cfg->data[cuidx], polladdr);
        }
        this->num_cus = cfg->num_cus;

        // Create KDMA CUs
        bool cdmaEnabled = false;
        if (xdevice->isCdmaEnabled()) {

            uint32_t addr = 0;
            for (unsigned int i = 0 ; i < 4; i++) { /* 4 is from xclfeatures.h */
                addr = xdevice->getCdmaBaseAddress(i);
                if (addr) {
                    xocl_cu *xcu = this->cus[cuidx];
                    uint64_t polladdr = (ert_poll) 					
                        ? this->cq_base + (cuidx+1) * cfg->slot_size : 0;

                    if (!xcu)
                        xcu = this->cus[cuidx] = new xocl_cu(xdevice);;
                    xcu->cu_init(cuidx, this->base, addr, polladdr);

                    cdmaEnabled = true;
                    ++this->num_cus;
                    ++this->num_cdma;
                    ++cfg->num_cus;
                    ++cfg->count;
                    cfg->data[cuidx] = addr;
                    ++cuidx;
                }
            }
        }

        if ((ert_full || ert_poll) && !this->ert) {
            this->ert = new xocl_ert(xdevice, this->csr_base, this->cq_base);
        }

        if (ert_poll) {
            SCHED_INFO("configuring dataflow mode with ert polling\n");
            cfg->slot_size = this->cq_size / MAX_CUS;
            cfg->cu_isr = 0;
            cfg->cu_dma = 0;
            this->ert->ert_cfg(this->cq_size, MAX_CUS, cfg->cq_int);
            this->ops = new ert_poll_ops(this);
            this->polling_mode = 1; //cfg->polling; 
        } else if (ert_full) {
            SCHED_INFO("configuring embedded scheduler mode\n");
            this->ert->ert_cfg(this->cq_size, ert_num_slots, cfg->cq_int);
            this->ops = new ert_ops(this);
            this->polling_mode = 1; //cfg->polling;
            this->cq_interrupt = cfg->cq_int;
            cfg->cu_dma = 0;
            cfg->cdma = cdmaEnabled ? 1 : 0;
        } else {
            SCHED_INFO("configuring penguin scheduler mode\n");
            this->ops = new penguin_ops(this);
            this->polling_mode = true;
        }

        // The KDS side of of the scheduler is now configured.  If ERT is
        // enabled, then the configure command will be started asynchronously
        // on ERT.  The shceduler is not marked configured until ERT has
        // completed 
        this->configure_active = true;


        SCHED_INFO("scheduler config ert(%d), dataflow(%d), slots(%d), cudma(%d), cuisr(%d), cdma(%d), cus(%d)\n"
                , ert_poll | ert_full
                , cfg->dataflow
                , ert_num_slots
                , cfg->cu_dma ? 1 : 0
                , cfg->cu_isr ? 1 : 0
                , this->num_cdma
                , this->num_cus);

        return 0;
    }

    /**
     * Check if exec core is in full ERT mode
     */
    bool exec_core::exec_is_ert()
    {
        return this->ops->is_ert();
    }

    /**
     * Check if exec core is in full ERT poll mode
     */
    bool exec_core::exec_is_ert_poll()
    {
        return this->ops->is_ert_poll();
    }

    /**
     * Check if exec core is in penguin mode
     */
    bool exec_core::exec_is_penguin()
    {
        return this->ops->is_penguin();
    }

    /**
     * Check if exec core is in polling mode
     */
    bool exec_core::exec_is_polling()
    {
        return this->polling_mode;
    }

    /**
     * Check if exec core has been requested to flush commands
     */
    bool exec_core::exec_is_flush()
    {
        return this->flush;
    }

    /**
     * Get base address of a CU
     */
    uint32_t exec_core::exec_cu_base_addr(uint32_t cuidx)
    {
        return this->cus[cuidx]->cu_base_addr();
    }

    /**
    */
    uint32_t exec_core::exec_cu_usage(uint32_t cuidx)
    {
        return this->cu_usage[cuidx];
    }

    uint32_t exec_core::exec_cu_status(uint32_t cuidx)
    {
        return this->cu_status[cuidx];
    }

    uint32_t exec_core::exec_num_running()
    {
        return this->num_running_cmds;
    }

    uint32_t exec_core::exec_num_pending()
    {
        return this->num_pending_cmds;
    }

    bool exec_core::exec_valid_cu(uint32_t cuidx)
    {
        xocl_cu *xcu = this->cus[cuidx];
        return xcu ? xcu->cu_valid() : false;
    }

    /**
    */
    void exec_core::exec_cfg()
    {
    }

    xocl_scheduler * exec_core::exec_scheduler()
    {
        return this->scheduler;
    }

    void exec_core::exec_update_custatus()
    {
        SCHED_DEBUGF("-> %s\n", __func__);
        uint32_t cuidx;
        // ignore kdma which on least at u200_2018_30_1 is not BAR mapped
        for (cuidx = 0; cuidx < this->num_cus - this->num_cdma; ++cuidx) 
        {
            // skip free running kernels which is not BAR mapped
            if (!this->exec_valid_cu(cuidx))
                this->cu_status[cuidx] = 0;
            else if (this->exec_is_ert())
                this->cu_status[cuidx] = ert->ert_cu_status(cuidx)
                    ? AP_START : AP_IDLE;
            else
                this->cu_status[cuidx] = this->cus[cuidx]->cu_status();
        }

        // reset cdma status
        for (; cuidx < this->num_cus; ++cuidx)
            this->cu_status[cuidx] = 0;

        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * finish_cmd() - Special post processing of commands after execution
     */
    int exec_core::exec_finish_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s\n", __func__);

        if (xcmd->opcode() == ERT_CONFIGURE) {
            this->configured = true;
            this->configure_active = false;
            return 0;
        }

        if (xcmd->opcode() != ERT_CU_STAT)
            return 0;

        if (exec_is_ert())
            ert->ert_read_custat(xcmd, this->num_cus);

        //exec_update_custatus();

        SCHED_DEBUGF("<- %s\n", __func__);
        return 0;
    }

    /*
     * execute_copbo_cmd() - Execute ERT_START_COPYBO commands
     *
     * This is special case for copying P2P
     */
    int exec_core::exec_execute_copybo_cmd(xocl_cmd *xcmd)
    {
#if 0
        SCHED_DEBUGF("-> %s\n", __func__);
        int ret = 0;
        assert(false);
        struct ert_start_copybo_cmd *ecmd = xcmd->ert_cp;
        struct drm_file *filp = (struct drm_file *)ecmd->arg;
        struct drm_device *ddev = filp->minor->dev;

        SCHED_DEBUGF("-> %s(%d,%lu)\n", __func__, this->uid, xcmd->uid);
        ret = xocl_copy_import_bo(ddev, filp, ecmd);
        SCHED_DEBUGF("<- %s\n", __func__);
        return ret == 0 ? 0 : 1;
#endif
        return 0;
    }

    /*
     * notify_host() - Notify user space that a command is complete.
     *
     * Update outstanding execs count for client and device.
     */
    void exec_core::exec_notify_host(xocl_cmd* xcmd)
    {
        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * exec_cmd_mark_complete() - Move a command to specified state and notify host
     *
     * Commands are marked complete in two ways
     *  1. Through polling (of CUs or of MB status register)
     *  2. Through interrupts from MB
     *
     * @xcmd: Command to mark complete
     * @state: New command state
     *
     * The external command state is changed to @state and the host is notified
     * that some command has completed.  The calling code is responsible for
     * recycling / freeing the command, this function *cannot* call cmd_free
     * because when ERT is enabled multiple commands can complete in one shot and
     * list iterations of running cmds (@exec_running_to_complete) would not work.
     */
    void exec_core::exec_mark_cmd_state(xocl_cmd *xcmd, enum ert_cmd_state state)
    {
        SCHED_DEBUGF("-> %s exec(%d) xcmd(%lu) state(%d)\n",
                __func__, this->uid, xcmd->uid, state);
        if (xcmd->type() == ERT_CTRL)
            exec_finish_cmd(xcmd);

        if (xcmd->cu_idx != no_index)
            --this->cu_load_count[xcmd->cu_idx];

        xcmd->set_state(state);

        if (this->polling_mode)
            scheduler->scheduler_decr_poll();

        if (this->ert) 
            ert->ert_release_slot(xcmd);

        exec_notify_host(xcmd);

        SCHED_DEBUGF("<- %s\n", __func__);
    }

    void exec_core::exec_mark_cmd_complete(xocl_cmd *xcmd)
    {
        exec_mark_cmd_state(xcmd,
                xcmd->aborted ? ERT_CMD_STATE_ABORT : ERT_CMD_STATE_COMPLETED);
    }

    void exec_core::exec_mark_cmd_error(xocl_cmd *xcmd)
    {
        exec_mark_cmd_state(xcmd,
                xcmd->aborted ? ERT_CMD_STATE_ABORT : ERT_CMD_STATE_ERROR);
    }

    /**
     * process_cmd_mask() - Move all commands in mask to complete state
     *
     * @mask: Bitmask with queried statuses of commands
     * @mask_idx: Index of the command mask. Used to offset the actual cmd slot index
     *
     * scheduler_ops ERT mode callback function
     *
     * Used in ERT mode only.
     */
    void exec_core::exec_process_cmd_mask(uint32_t mask, uint32_t mask_idx)
    {
        int bit_idx = 0, cmd_idx = 0;

        SCHED_DEBUGF("-> %s(0x%x,%d)\n", __func__, mask, mask_idx);

        for (bit_idx = 0, cmd_idx = mask_idx<<5; bit_idx < 32; mask >>= 1, ++bit_idx, ++cmd_idx) {
            xocl_cmd *xcmd = (mask & 0x1)
                ? ert->ert_get_cmd(cmd_idx)
                : NULL;

            if (xcmd)
                exec_mark_cmd_complete(xcmd);
        }

        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * process_cu_mask() - Check status of compute units per mask
     *
     * @mask: Bitmask with CUs to check
     * @mask_idx: Index of the CU mask. Used to offset the actual CU index
     *
     * scheduler_ops ERT poll mode callback function
     *
     * Used in ERT CU polling mode only.  When ERT interrupts host it is because
     * some CUs changed state when ERT polled it.  These CUs must be checked by
     * KDS and if a command has completed then it must be marked complete.
     *
     * CU indices in mask are offset by 1 to reserve CQ slot 0 for ctrl cmds
     */
    void exec_core::exec_process_cu_mask(uint32_t mask, uint32_t mask_idx)
    {
        int bit_idx = 0, cu_idx = 0;

        SCHED_DEBUGF("-> %s(0x%x,%d)\n", __func__, mask, mask_idx);
        for (bit_idx = 0, cu_idx = mask_idx<<5; bit_idx < 32; mask >>= 1, ++bit_idx, ++cu_idx) {
            xocl_cu  *xcu;
            xocl_cmd *xcmd;

            if (!(mask & 0x1))
                continue;

            xcu = this->cus[cu_idx-1]; // note offset

            // poll may have been done outside of ERT when a CU was
            // started; alas there can be more than one completed cmd
            while ((xcmd = xcu->cu_first_done())) {
                xcu->cu_pop_done();
                exec_mark_cmd_complete(xcmd);
            }
        }
        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * exec_penguin_start_cu_cmd() - Callback in penguin and dataflow mode
     *
     * @xcmd: command to start
     *
     * scheduler_ops penguin and ert poll callback function for CU type commands
     *
     * Used in penguin and ert poll mode where KDS schedules and starts
     * compute units.
     */
    bool exec_core::exec_penguin_start_cu_cmd(xocl_cmd *xcmd)
    {
        xocl_cu *xcu = NULL;

        SCHED_DEBUGF("-> %s cmd(%lu)\n", __func__, xcmd->uid);

        // CU was selected when command was submitted
        xcu = this->cus[xcmd->cu_idx];
        if (xcu->cu_ready() && xcu->cu_start(xcmd)) 
        {
            xcmd->set_int_state(ERT_CMD_STATE_RUNNING);
            this->running_cmd_queue.push_back(xcmd);
            ++this->num_running_cmds;
            ++this->cu_usage[xcmd->cu_idx];

            SCHED_DEBUGF("<- %s -> true\n", __func__);
            return true;
        }

        SCHED_DEBUGF("<- %s -> false\n", __func__);
        return false;
    }

    /**
     * exec_penguin_start_ctrl_cmd() - Callback in penguin mode for ctrl commands
     *
     * In penguin mode ctrl commands run synchronously, so mark them complete when
     * done, e.g. there is nothihng to poll for completion as there is nothing
     * left running
     */
    bool exec_core::exec_penguin_start_ctrl_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d)\n", __func__, this->uid);

        // Nothting to do for currently supported ctrl commands
        // Just mark the command as complete and free it.
        exec_mark_cmd_complete(xcmd);
        exec_cmd_free(xcmd);

        SCHED_DEBUGF("<- %s returns true\n", __func__);

        return true;
    }

    /**
     * penguin_query() - Check command status of argument command
     *
     * @exec: device
     * @xcmd: command to check
     *
     * scheduler_ops penguin mode callback function
     *
     * Function is called in penguin mode where KDS polls CUs for completion
     */
    void exec_core::exec_penguin_query_cmd(xocl_cmd *xcmd)
    {
        uint32_t cmdtype = xcmd->type();

        SCHED_DEBUGF("-> %s cmd(%lu) opcode(%d) type(%d) slot_idx=%d\n",
                __func__, xcmd->uid, xcmd->opcode(), cmdtype, xcmd->slot_idx);

        if (cmdtype == ERT_CU) {
            xocl_cu *xcu = this->cus[xcmd->cu_idx];

            if (xcu->cu_first_done() == xcmd) {
                xcu->cu_pop_done();
                exec_mark_cmd_complete(xcmd);
            }
        }

        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * ert_ert_start_cmd() - Start a command in ERT mode
     *
     * @xcmd: command to start
     *
     * scheduler_ops ERT mode callback function
     *
     * Used in ert mode where ERT schedules, starts, and polls compute units.
     */
    bool exec_core::exec_ert_start_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu) opcode(%d)\n", __func__,
                this->uid, xcmd->uid, xcmd->opcode());

        if (ert->ert_start_cmd(xcmd)) 
        {
            xcmd->set_int_state(ERT_CMD_STATE_RUNNING);
            this->running_cmd_queue.push_back(xcmd);
            ++this->num_running_cmds;

            SCHED_DEBUGF("<- %s returns true\n", __func__);
            return true;
        }

        // start failed
        SCHED_DEBUGF("<- %s returns false\n", __func__);
        return false;
    }

    /**
     * exec_ert_start_ctrl_cmd() - Callback in ERT mode for ctrl commands
     *
     * In ERT poll mode cu stats are managed by kds itself, nothing
     * to retrieve from ERT.  This could be split to two functions
     * through scheduler_ops, but not really critical.
     */
    bool exec_core::exec_ert_start_ctrl_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d) xcmd(%lu)\n", __func__, this->uid, xcmd->uid);

        // For CU_STAT in ert polling mode (dataflow assisted polling) there
        // is nothing to do, mark complete immediately
        if (xcmd->opcode() == ERT_CU_STAT && exec_is_ert_poll()) {
            exec_mark_cmd_complete(xcmd);
            exec_cmd_free(xcmd);
            return true;
        }

        // Pass all other control commands to ERT
        if (exec_ert_start_cmd(xcmd)) {
            SCHED_DEBUGF("<- %s returns true\n", __func__);
            return true;
        }

        SCHED_DEBUGF("<- %s returns false\n", __func__);
        return false;
    }

    /**
     * Clear the ERT command queue status register
     *
     * This can be necessary in ert polling mode, where KDS itself
     * can be ahead of ERT, so stale interrupts are possible which
     * is bad during reconfig.
     */
    void exec_core::exec_ert_clear_csr()
    {
        if (exec_is_ert() || exec_is_ert_poll())
            ert->ert_clear_csr();
    }

    /**
     * exec_ert_query_mailbox() - Check ERT CQ completion mailbox
     *
     * @exec: device
     * @xcmd: command to check
     *
     * This function is for ERT and ERT polling mode.  When KDS is configured to
     * poll, this function polls the ert->host mailbox.
     *
     * The function checks all available entries in the mailbox so more than one
     * command may be marked complete by this function.
     */
    void exec_core::exec_ert_query_mailbox(xocl_cmd *xcmd)
    {
#if 0
        uint32_t mask;
        uint32_t cmdtype = xcmd->type();
        uint32_t slot;
        int mask_idx;
        uint32_t slots[MAX_SLOTS];
        uint32_t cnt = 0;
        int i;

        SCHED_DEBUGF("-> %s cmd(%lu)\n", __func__, xcmd->uid);
        while (!(xocl_mailbox_versal_get(xcmd->xdev, &slot)))
            slots[cnt++] = slot;

        if (!cnt)
            return;

        for (i = 0; i < cnt; i++) {
            // special case for control commands which are in slot 0
            if (cmdtype == ERT_CTRL && (slots[i] == 0)) {
                exec_process_cmd_mask(exec, 0x1, 0);
                continue;
            }

            mask = 1 << (slots[i] % 32);
            mask_idx = slots[i] >> 5;

            this->ops->process_mask(mask, mask_idx);
        }
#endif
        SCHED_DEBUGF("<- %s\n", __func__);
    }


    /**
     * ert_query_csr() - Check ERT CQ completion register
     *
     * @exec: device
     * @xcmd: command to check
     * @mask_idx: index of status register to check
     *
     * This function is for ERT and ERT polling mode.  When KDS is configured to
     * poll, this function polls the command queue completion register from
     * ERT. In interrupt mode check the interrupting status register.
     *
     * The function checks all entries in the same command queue status register as
     * argument command so more than one command may be marked complete by this
     * function.
     */
    void exec_core::exec_ert_query_csr(xocl_cmd *xcmd, uint32_t mask_idx)
    {
        uint32_t mask = 0;
        uint32_t cmdtype = xcmd->type();

        SCHED_DEBUGF("-> %s cmd(%lu), mask_idx(%d)\n", __func__, xcmd->uid, mask_idx);

        if (this->polling_mode
                || (mask_idx == 0 && sr0.exchange(0))
                || (mask_idx == 1 && sr1.exchange(0))
                || (mask_idx == 2 && sr2.exchange(0))
                || (mask_idx == 3 && sr3.exchange(0))) 
        {
            uint32_t csr_addr = this->csr_base + (mask_idx<<2);
            mask = ioread32(csr_addr);
            SCHED_DEBUGF("++ %s csr_addr=0x%x mask=0x%x\n", __func__, csr_addr, mask);
        }

        if (!mask) {
            SCHED_DEBUGF("<- %s mask(0x0)\n", __func__);
            return;
        }

        // special case for control commands which are in slot 0
        if (cmdtype == ERT_CTRL && (mask & 0x1)) {
            exec_process_cmd_mask(0x1, mask_idx);
            mask ^= 0x1;
        }

        if (mask)
            this->ops->process_mask(mask, mask_idx);

        SCHED_DEBUGF("<- %s\n", __func__);
    }


    /**
     * exec_ert_query_cu() - Callback for ERT poll mode
     *
     * @xcmd: command to check
     *
     * ERT assisted polling in dataflow mode
     *
     * NOTE: in ERT poll mode the CQ slot indices are offset by 1 for cu indices,
     * this is done so as to reserve slot 0 for control commands.
     *
     * In ERT poll mode, the command completion register corresponds to compute
     * units, which ERT is monitoring / polling for completion.
     *
     * If a CU status has changed, ERT will notify host via 4 interrupt registers
     * each representing 32 CUs.  This function checks the interrupt register
     * containing the CU on which argument cmd was started.
     *
     * The function checks all entries in the same status register as argument
     * command so more than one command may be marked complete by this function.
     */
    void exec_core::exec_ert_query_cu(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s cmd(%lu), cu_idx(%d)\n", __func__, xcmd->uid, xcmd->cu_idx);
        exec_ert_query_csr(xcmd, (xcmd->cu_idx+1) >> 5); // note offset
        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * exec_ert_query_cmd() - Callback for cmd completion when ERT mode
     *
     * @xcmd: command to check
     *
     * ERT CU scheduling mode
     *
     * In ERT mode, the command completion register corresponds to ERT commands,
     * which KDS wrote to the ERT command queue when a command was started.
     *
     * If a command has completed, ERT will notify host via 4 interrupt registers
     * each representing 32 CUs.  This function checks the interrupt register
     * containing the CU on which argument cmd was started.
     *
     * If a CU status has changed, ERT will notify host via 4 interrupt registers
     * each representing 32 commands.  This function checks the interrupt register
     * containing the argument command.
     *
     * The function checks all entries in the same status register as argument
     * command so more than one command may be marked complete by this function.
     */
    void exec_core::exec_ert_query_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s cmd(%lu), slot_idx(%d)\n", __func__, xcmd->uid, xcmd->slot_idx);

        //if (XOCL_DSA_IS_VERSAL(xdev)) {
        //	exec_ert_query_mailbox(xcmd);
        //} else
        exec_ert_query_csr(xcmd, (xcmd->slot_idx) >> 5);

        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * query_cmd() - Check status of command
     *
     * Function dispatches based on penguin vs ert mode.  In ERT mode
     * multiple commands can be marked complete by this function.
     */
    void exec_core::exec_query_cmd(xocl_cmd *xcmd)
    {
        uint32_t cmdtype = xcmd->type();

        SCHED_DEBUGF("-> %s cmd(%lu)\n", __func__, xcmd->uid);

        // ctrl commands may need special attention
        if (cmdtype == ERT_CTRL)
            this->ops->query_ctrl(xcmd);
        else
            this->ops->query_cmd(xcmd);

        SCHED_DEBUGF("<- %s\n", __func__);
    }

    void exec_core::exec_cmd_free(xocl_cmd* xcmd)
    {
        //! Release the xcmd to object pool
        scheduler->cmd_pool.destroy(xcmd);
    }

    void exec_core::exec_abort_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, this->uid, xcmd->uid);
        exec_notify_host(xcmd);
        exec_cmd_free(xcmd);
        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * start_cmd() - Start execution of a command
     *
     * Return: true if succesfully started, false otherwise
     *
     * Function dispatches based on penguin vs ert mode
     */
    bool exec_core::exec_start_cu_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu) opcode(%d)\n", __func__,
                this->uid, xcmd->uid, xcmd->opcode());

        if (this->ops->start_cmd(xcmd)) {
            SCHED_DEBUGF("<- %s returns true\n", __func__);
            return true;
        }

        SCHED_DEBUGF("<- %s returns false\n", __func__);
        return false;
    }

    /**
     * start_start_ctrl_cmd() - Start execution of a command
     *
     * Return: true if succesfully started, false otherwise
     *
     * Function dispatches based on penguin vs ert mode
     */
    bool exec_core::exec_start_ctrl_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu) opcode(%d)\n", __func__,
                this->uid, xcmd->uid, xcmd->opcode());

        // Let scheduler mode determine the needed processing
        if (this->ops->start_ctrl(xcmd)) {
            SCHED_DEBUGF("<- %s returns true\n", __func__);
            return true;
        }

        SCHED_DEBUGF("<- %s returns false\n", __func__);
        return false;
    }



    /**
     * exec_start_kds_cmd() - KDS commands run synchronously
     */
    bool exec_core::exec_start_kds_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu) opcode(%d)\n", __func__,
                this->uid, xcmd->uid, xcmd->opcode());

        // Let scheduler mode determine the needed processing (currently none)
        // if (this->ops->start_kds(xcmd) {...}

        // kds commands are locally processed so are now complete
        exec_mark_cmd_complete(xcmd);
        exec_cmd_free(xcmd);
        SCHED_DEBUGF("<- %s returns true\n", __func__);
        return true;
    }

    int exec_core::exec_start_cu_range(uint32_t start, uint32_t end)
    {
        uint32_t started = 0;
        uint32_t cuidx;

        for (cuidx = start; cuidx < end; ++cuidx) 
        {
            if(this->pending_cu_queue[cuidx].empty()) continue;

            xocl_cmd *xcmd = this->pending_cu_queue[cuidx].front();
            if (exec_start_cu_cmd(xcmd)) {
                this->pending_cu_queue[cuidx].pop();
                ++started;
            }
        }

        return started;
    }

    int exec_core::exec_start_cus()
    {
        static uint32_t first_cu = -1;
        uint32_t start_cu = (first_cu < this->num_cus) ? ++first_cu : (first_cu = 0);

        uint32_t total = 0;
        uint32_t prev = 0;

        SCHED_DEBUGF("-> %s first_cu(%d) start_cu(%d)\n", __func__, first_cu, start_cu);

        do {
            prev = total;
            total += exec_start_cu_range(start_cu, this->num_cus);
            total += exec_start_cu_range(0, start_cu);
        } while (total > prev);

        return total;
    }

    int exec_core::exec_start_ctrl()
    {
        if(this->pending_ctrl_queue.empty()) return 0;

        xocl_cmd *xcmd = this->pending_ctrl_queue.front();
        if(exec_start_ctrl_cmd(xcmd)) {
            this->pending_ctrl_queue.pop();
            return 1;
        }

        return 0;
    }

    int exec_core::exec_start_kds()
    {
        if(this->pending_kds_queue.empty()) return 0;

        xocl_cmd *xcmd = this->pending_kds_queue.front();
        if(exec_start_kds_cmd(xcmd)) {
            this->pending_kds_queue.pop();
            return 1;
        }

        return 0;
    }

    int  exec_core::exec_start_scu()
    {
        if(this->pending_scu_queue.empty()) return 0;

        xocl_cmd *xcmd = this->pending_scu_queue.front();
        if(exec_start_cu_cmd(xcmd)) {
            this->pending_scu_queue.pop();
            return 1;
        }

        return 0;
    }

    bool exec_core::exec_submit_cu_cmd(xocl_cmd *xcmd)
    {
        // Append cmd to end of shortest CU list
        uint32_t min_load_count = -1;
        uint32_t cuidx = MAX_CUS;
        uint32_t bit;
        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, this->uid, xcmd->uid);
        for (bit = xcmd->first_cu(); bit < this->num_cus; bit = xcmd->next_cu(bit)) {
            uint32_t load_count = this->cu_load_count[bit];
            SCHED_DEBUGF(" bit(%d) num_cus(%d) load_count(%d) min_load_count(%d)\n", bit, num_cus, load_count, min_load_count);
            if (load_count >= min_load_count)
                continue;
            cuidx = bit;
            if ((min_load_count = load_count) == 0)
                break;
        }

        if (cuidx < MAX_CUS) {
          this->pending_cu_queue[cuidx].push(xcmd);
          xcmd->set_cu(cuidx);
          ++this->cu_load_count[cuidx];
        }
        SCHED_DEBUGF("<- %s cuidx(%d) load(%d)\n", __func__, cuidx, this->cu_load_count[cuidx]);
        return true;
    }

    bool exec_core::exec_submit_ctrl_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, this->uid, xcmd->uid);

        // configure command should configure kds succesfully or be abandoned
        if (xcmd->opcode() == ERT_CONFIGURE && (this->configure_active || exec_cfg_cmd(xcmd))) {
            xcmd->set_state(ERT_CMD_STATE_ERROR);
            exec_abort_cmd(xcmd);
            SCHED_DEBUGF("<- %s returns false\n", __func__);
            return false;
        }

        this->pending_ctrl_queue.push(xcmd);

        SCHED_DEBUGF("<- %s true\n", __func__);
        return true;
    }

    bool exec_core::exec_submit_kds_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, this->uid, xcmd->uid);

        // If preprocessing fails, then abandon
        if (xcmd->opcode() == ERT_START_COPYBO && exec_execute_copybo_cmd(xcmd)) {
            xcmd->set_state(ERT_CMD_STATE_ERROR);
            exec_abort_cmd(xcmd);
            SCHED_DEBUGF("<- %s returns false\n", __func__);
            return false;
        }

        this->pending_kds_queue.push(xcmd);

        SCHED_DEBUGF("<- %s returns true\n", __func__);
        return true;
    }

    bool exec_core::exec_submit_scu_cmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, this->uid, xcmd->uid);

        this->pending_scu_queue.push(xcmd);

        SCHED_DEBUGF("<- %s returns true\n", __func__);
        return true;
    }

    bool exec_core::exec_submit_cmd(xocl_cmd *xcmd)
    {
        bool ret = false;

        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, this->uid, xcmd->uid);

        if (xcmd->type() == ERT_CU)
            ret = exec_submit_cu_cmd(xcmd);
        else if (xcmd->type() == ERT_KDS_LOCAL)
            ret = exec_submit_kds_cmd(xcmd);
        else if (xcmd->type() == ERT_CTRL)
            ret = exec_submit_ctrl_cmd(xcmd);
        else if (xcmd->type() == ERT_SCU)
            ret = exec_submit_scu_cmd(xcmd);
        else
            SCHED_DEBUGF("Unknown command type %d\n",xcmd->type());

        if (ret && this->polling_mode)
            scheduler->scheduler_incr_poll();

        if (ret)
            ++this->num_pending_cmds;

        SCHED_DEBUGF("<- %s ret(%d)\n", __func__, ret);
        return ret;
    }

    void exec_core::exec_error_to_free(xocl_cmd *xcmd)
    {
        exec_notify_host(xcmd);
        exec_cmd_free(xcmd);
    }

    void exec_core::exec_new_to_queued(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, this->uid, xcmd->uid);

        // add to core command queue
        this->pending_cmd_queue.push(xcmd);
        xcmd->set_int_state(ERT_CMD_STATE_QUEUED);
        SCHED_DEBUGF("<- %s\n", __func__);
    }

    void exec_core::exec_queued_to_submitted()
    {
        SCHED_DEBUGF("-> %s\n", __func__);

        while(!this->pending_cmd_queue.empty()) {
            xocl_cmd *xcmd = this->pending_cmd_queue.front();
            exec_submit_cmd(xcmd);

            this->pending_cmd_queue.pop();
        }
        SCHED_DEBUGF("<- %s\n", __func__);
    }

    void exec_core::exec_submitted_to_running()
    {
        uint32_t started = 0;
        SCHED_DEBUGF("-> %s exec(%d)\n", __func__, this->uid);

        started += this->exec_start_ctrl();
        started += this->exec_start_cus();
        started += this->exec_start_kds();
        started += this->exec_start_scu();
        this->num_pending_cmds -= started;

        // Force at least one iteration if in ert poll mode where kds can be
        // ahead of ert polling.  A pending interrupt has to be cleared before
        // new interrupts can be send by ERT.
        if (started && this->exec_is_ert_poll())
            scheduler->scheduler_intr();

        SCHED_DEBUGF("<- %s started(%d)\n", __func__, started);
    }

    void exec_core::exec_running_to_complete()
    {
        SCHED_DEBUGF("-> %s exec(%d)\n", __func__, this->uid);

        auto iter = running_cmd_queue.begin();
        auto end  = running_cmd_queue.end();
        while(iter != end)
        {
            xocl_cmd* xcmd = *iter;
            // guard against exec_query_cmd completing multiple commands
            // in one call when ert is enabled.
            if (xcmd->state == ERT_CMD_STATE_RUNNING)
                this->exec_query_cmd(xcmd);

            if (xcmd->state >= ERT_CMD_STATE_COMPLETED) {
                --this->num_running_cmds;
                iter = this->running_cmd_queue.erase(iter);
                exec_cmd_free(xcmd); 
            } else {
                iter++;
            }
        }
        SCHED_DEBUGF("<- %s\n", __func__);
    }

    void exec_core::exec_service_cmds()
    {
        SCHED_DEBUGF("-> %s exec(%d)\n", __func__, this->uid);
        // running commands
        exec_running_to_complete();

        // Submit new commands for execution
        exec_queued_to_submitted();

        // Start commands
        exec_submitted_to_running();
        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /******************************** xocl_scheduler *************************************/

    /**
     * Inilize xocl_scheduler and scheduler_thread 
     */ 
    xocl_scheduler::xocl_scheduler(HwEmShim* dev)
    {
        device = dev;
        error = false;
        stop = false;
        reset = false;
        poll = 0;
        intc = 0;
        num_pending = 0;

        exec = new exec_core(device, this);
        scheduler_thread = new std::thread(&xocl_scheduler::scheduler, this);
    }

    /**
     * cleanup scheduler_thread and other resources on exit
     */
    xocl_scheduler::~xocl_scheduler()
    {
        {
            std::unique_lock<std::mutex> lck(scheduler_mutex);
            this->stop = true;
            wait_condition.notify_all();
        }

        if(scheduler_thread->joinable()) {
            scheduler_thread->join(); //! Wait untill scheduler_thread exits
        }

        delete exec;
        SCHED_DEBUGF("scheduler_thread exited\n");
    }


    void xocl_scheduler::scheduler_wake_up()
    {
        SCHED_DEBUGF("-> %s\n", __func__);
        wait_condition.notify_all();
        SCHED_DEBUGF("<- %s\n", __func__);
    }


    void xocl_scheduler::scheduler_intr()
    {
        SCHED_DEBUGF("-> %s\n", __func__);

        std::unique_lock<std::mutex> lck(scheduler_mutex);
        this->intc = 1;
        scheduler_wake_up();

        SCHED_DEBUGF("<- %s\n", __func__);
    }

    void xocl_scheduler::scheduler_decr_poll()
    {
        SCHED_DEBUGF("-> %s\n", __func__);

        std::unique_lock<std::mutex> lck(scheduler_mutex);
        --this->poll;

        SCHED_DEBUGF("<- %s\n", __func__);
    }

    void xocl_scheduler::scheduler_incr_poll()
    {
        SCHED_DEBUGF("-> %s\n", __func__);

        std::unique_lock<std::mutex> lck(scheduler_mutex);
        ++this->poll;

        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * scheduler_queue_cmds() - Dispatch pending commands to cores
     */
    void xocl_scheduler::scheduler_queue_cmds()
    {
        if(pending_cmds.empty()) return;

        SCHED_DEBUGF("-> %s\n", __func__);
        std::lock_guard<std::mutex> lk(pending_cmds_mutex);

        for(auto& xcmd : pending_cmds) 
        {
            SCHED_DEBUGF("+ dispatching cmd(%lu)\n", xcmd->uid);

            // move command to proper execution core
            exec->exec_new_to_queued(xcmd);

            num_pending--; //atomic decrement
        }
        pending_cmds.clear();

        SCHED_DEBUGF("<- %s\n", __func__);
    }


    /**
     * scheduler_service_cores() - Iterate all devices
     */
    void xocl_scheduler::scheduler_service_cores()
    {
        SCHED_DEBUGF("-> %s\n", __func__);
        exec->exec_service_cmds();
        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * scheduler_wait_condition() - Check status of scheduler wait condition
     *
     * Scheduler must wait (sleep) if
     *   1. there are no pending commands
     *   2. no pending interrupt from embedded scheduler
     *   3. no pending complete commands in polling mode
     *
     * Return: 1 if scheduler must wait, 0 othewise
     */
    int xocl_scheduler::scheduler_wait_condition()
    {
        if (num_pending) {
            SCHED_DEBUGF("scheduler wakes to copy new pending commands\n");
            return 0;
        }

        if (this->intc) {
            SCHED_DEBUGF("scheduler wakes on interrupt\n");
            this->intc = 0;
            return 0;
        }

        if (this->poll) {
            SCHED_DEBUGF("scheduler wakes to poll(%d)\n", this->poll);
            return 0;
        }

        SCHED_DEBUGF("scheduler waits ...\n");
        return 1;
    }

    /**
     * scheduler_wait() - check if scheduler should wait
     *
     * See scheduler_wait_condition().
     */
    void xocl_scheduler::scheduler_wait()
    {
        SCHED_DEBUGF("-> %s\n", __func__);

        std::unique_lock<std::mutex> lck(scheduler_mutex);
        wait_condition.wait_for(lck, std::chrono::milliseconds(20), 
                [this] { return scheduler_wait_condition() == 0; });

        SCHED_DEBUGF("<- %s\n", __func__);
    }

    /**
     * scheduler_loop() - Run one loop of the scheduler
     */
    void xocl_scheduler::scheduler_loop()
    {
        SCHED_DEBUGF("%s\n", __func__);
        scheduler_wait();

        if (this->error) {
            SCHED_DEBUGF("scheduler encountered unexpected error\n");
            return;
        }

        if (this->stop)
            return;

        if (this->reset) {
            SCHED_DEBUGF("scheduler is resetting after timeout\n");
        }

        // queue new pending commands
        scheduler_queue_cmds();

        // iterate all execution cores
        scheduler_service_cores();
    }

    /**
     * scheduler() - Command scheduler thread routine
     */
    int xocl_scheduler::scheduler()
    {
        while (!this->stop && !this->error)
            scheduler_loop();

        SCHED_DEBUGF("%s thread exits with value %d\n", __func__, this->error);
        return this->error ? 1 : 0;
    }


    /**
     * add_xcmd() - Add initialized xcmd object to pending command list
     *
     * @xcmd: Command to add
     *
     * Scheduler copies pending commands to its internal command queue.
     *
     * Return: 0 on success
     */
    int xocl_scheduler::add_xcmd(xocl_cmd *xcmd)
    {
        SCHED_DEBUGF("-> %s cmd(%lu)\n", __func__, xcmd->uid);

        if ((!exec->configured && xcmd->opcode() != ERT_CONFIGURE)) {
            SCHED_DEBUGF("scheduler can't add cmd(%lu) opcode(%d)  exec confgured(%d)\n",
                    xcmd->uid, xcmd->opcode(),  exec->configured);
            return 1;;
        }
        xcmd->set_state(ERT_CMD_STATE_NEW);

        {
            //std::lock_guard<std::mutex> lk(pending_cmds_mutex);
            pending_cmds.push_back(xcmd);
        }

        num_pending++;
        scheduler_wake_up();

        SCHED_DEBUGF("<- %s ret(0) opcode(%d) type(%d)\n",
                __func__, xcmd->opcode(), xcmd->type());
        return 0;
    }


    /**
     * add_bo_cmd() - Add a new buffer object command to pending list
     *
     * @exec: Targeted device
     * @client: Client context
     * @bo: Buffer objects from user space from which new command is created
     * @numdeps: Number of dependencies for this command
     * @deps: List of @numdeps dependencies
     *
     * Scheduler copies pending commands to its internal command queue.
     *
     * Return: 0 on success, 1 on failure
     */
    int xocl_scheduler::add_bo_cmd(xclemulation::drm_xocl_bo *buf)
    {
        std::lock_guard<std::mutex> lk(pending_cmds_mutex);
        //! Get the command from boost object pool
        xocl_cmd *xcmd = cmd_pool.construct();

        if (!xcmd)
            return 1;

        SCHED_DEBUGF("-> %s cmd(%lu)\n", __func__, xcmd->uid);

        xcmd->bo_init(buf);

        if (add_xcmd(xcmd)) {
            SCHED_DEBUGF("<- %s ret(1) opcode(%d) type(%d)\n", __func__, xcmd->opcode(), xcmd->type());
            return 1;
        }

        SCHED_DEBUGF("<- %s ret(0) opcode(%d) type(%d)\n", __func__, xcmd->opcode(), xcmd->type());
        return 0;
    }

    /**
     * Entry point for exec buffer.
     *
     * Function adds exec buffer to the pending list of commands
     */
    int xocl_scheduler::add_exec_buffer(xclemulation::drm_xocl_bo *buf)
    {
        // Add the command to pending list
        return add_bo_cmd(buf);
    }

} // end xcl_hwemu 
