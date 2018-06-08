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

enum XOCL_QDMA_IOC_TYPES {
	XOCL_QDMA_CREATE_QUEUE,
	XOCL_QDMA_DESTROY_QUEUE,
	XOCL_QDMA_MODIFY_QUEUE,
	XOCL_QDMA_POST_WR,
	XOCL_QDMA_MAX
};

enum XOCL_QDMA_QUEUE_STATE {
	XOCL_QDMA_QSTATE_STOPPED,
	XOCL_QDMA_QSTATE_STARTED,
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
	uint32_t		qsize;		/* number of desc */
	uint32_t		desc_size;	/* size of each desc */
	uint64_t		flags;		/* isr en, wb en, etc */
	uint64_t		handle;		/* return of q handle */
};

/**
 * struct xocl_qdma_ioc_destroy_queue - Destroy streaming queue
 * used with XOCL_QDMA_DESTROY_QUEUE
 *
 * @handle:	queue handle returned by the driver
 */
struct xocl_qdma_ioc_destroy_queue {
	uint64_t		handle;
};

/**
 * struct xocl_qdma_ioc_modify_queue - Modify streaming queue
 * used with XOCL_QDMA_MODIFY_QUEUE
 *
 * @handle:	queue handle returned by the driver
 */
struct xocl_qdma_ioc_modify_queue {
	uint64_t		handle;
	uint32_t		state;		/* started or stopped */
	uint64_t		rid;
};

/**
 * struct xocl_qdma_ioc_post_wr - Read / Write streaming queue
 * used with XOCL_QDMA_IOC_POST_WR
 *
 * @handle:	queue handle returned by the driver
 */
struct xocl_qdma_ioc_post_wr {
	uint64_t		handle;
	uint32_t		op_code;	/* read, write etc */
	uint64_t		buf;
	uint64_t		buf_len;
	uint64_t		sgl;
	uint32_t		sgl_len;
	uint32_t		flags;
};

/**
 * ioctls numbers
 */
#define	XOCL_QDMA_IOC_CREATE_QUEUE		_IO(XOCL_QDMA_IOC_MAGIC, \
	XOCL_QDMA_CREATE_QUEUE)
#define	XOCL_QDMA_IOC_DESTROY_QUEUE		_IO(XOCL_QDMA_IOC_MAGIC, \
	XOCL_QDMA_DESTROY_QUEUE)
#define	XOCL_QDMA_IOC_MODIFY_QUEUE		_IO(XOCL_QDMA_IOC_MAGIC, \
	XOCL_QDMA_MODIFY_QUEUE)
#define	XOCL_QDMA_IOC_POST_WR			_IO(XOCL_QDMA_IOC_MAGIC, \
	XOCL_QDMA_POST_WR)
#endif
