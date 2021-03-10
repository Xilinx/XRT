/*
 *  Copyright (C) 2018, Xilinx Inc
 *
 *  This file is dual licensed.  It may be redistributed and/or modified
 *  under the terms of the Apache 2.0 License OR version 2 of the GNU
 *  General Public License.
 *
 *  Apache License Verbiage
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  GPL license Verbiage:
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by the Free Software Foundation;
 *  either version 2 of the License, or (at your option) any later version.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#ifndef	_XCL_QDMA_IOCTL_H_
#define	_XCL_QDMA_IOCTL_H_

#define	XOCL_QDMA_IOC_MAGIC		'Q'
#define	XOCL_QDMA_QUEUE_IOC_MAGIC	'q'

#define XOCL_INVALID_ROUTE_ID		-1;
#define	XOCL_INVALID_FLOW_ID		-1;

enum XOCL_QDMA_IOC_TYPES {
	XOCL_QDMA_CREATE_QUEUE,
	XOCL_QDMA_ALLOC_BUFFER,
	XOCL_QDMA_MAX
};

enum XOCL_QDMA_QUEUE_IOC_TYPES {
	XOCL_QDMA_QUEUE_MODIFY,
	XOCL_QDMA_QUEUE_FLUSH,
	XOCL_QDMA_QUEUE_MAX
};

enum XOCL_QDMA_QUEUE_STATE {
	XOCL_QDMA_QSTATE_STOPPED,
	XOCL_QDMA_QSTATE_STARTED,
};

/* has to keep in sync with xrt and opencl flags */
enum XOCL_QDMA_REQ_FLAG {
	XOCL_QDMA_REQ_FLAG_EOT		= (1 << 0),
	XOCL_QDMA_REQ_FLAG_CDH		= (1 << 1),
	XOCL_QDMA_REQ_FLAG_SILENT	= (1 << 3),
};

enum XOCL_QDMA_QUEUE_FLAG {
	XOCL_QDMA_QUEUE_FLAG_POLLING	= (1 << 2),
};

/**
 * struct xocl_qdma_ioc_create_queue - Create streaming queue
 * used with XOCL_QDMA_IOC_CREATE_QUEUE ioctl
 *
 * @handle:	queue handle returned by the driver
 */
struct xocl_qdma_ioc_create_queue {
	uint32_t		write;		/* read or write */
	uint32_t		pkt_mode;	/* stream or packet */
	uint64_t		rid;		/* route id */
	uint64_t		flowid;
	uint32_t		qsize;		/* number of desc */
	uint32_t		desc_size;	/* size of each desc */
	uint64_t		flags;		/* isr en, wb en, etc */
	uint64_t		handle;		/* out: queue handle */
};

/**
 * struct xocl_qdma_ioc_alloc_buf - Allocate DMA buffer
 *
 * @buf_fd
 * @size
 */
struct xocl_qdma_ioc_alloc_buf {
	size_t		size;
	int		buf_fd;
};

/**
 * struct xocl_qdma_req_header - per request header for out bind data
 *
 */
struct xocl_qdma_req_header {
	uint64_t	flags;		/* EOT, etc */
};

/**
 * ioctls numbers
 */
#define	XOCL_QDMA_IOC_CREATE_QUEUE		_IO(XOCL_QDMA_IOC_MAGIC, \
	XOCL_QDMA_CREATE_QUEUE)
#define	XOCL_QDMA_IOC_ALLOC_BUFFER		_IO(XOCL_QDMA_IOC_MAGIC, \
	XOCL_QDMA_ALLOC_BUFFER)

#define XOCL_QDMA_IOC_QUEUE_FLUSH		_IO(XOCL_QDMA_QUEUE_IOC_MAGIC,\
	XOCL_QDMA_QUEUE_FLUSH)
#define	XOCL_QDMA_IOC_QUEUE_MODIFY		_IO(XOCL_QDMA_QUEUE_IOC_MAGIC, \
	XOCL_QDMA_QUEUE_MODIFY)
#endif
