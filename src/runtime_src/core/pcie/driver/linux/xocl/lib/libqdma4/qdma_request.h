#ifndef __QDMA4_REQUEST_H__
#define __QDMA4_REQUEST_H__

#include "libqdma4_export.h"
#include "qdma_descq.h"

int qdma4_req_copy_fl(struct qdma_sw_sg *fsgl, unsigned int fsgcnt,
			struct qdma_request *req, unsigned int *copied_p);
int qdma_req_find_offset(struct qdma_request *req, bool use_dma_addr);
int qdma4_request_map(struct pci_dev *pdev, struct qdma_request *req);
void qdma4_request_unmap(struct pci_dev *pdev, struct qdma_request *req);

void qdma4_request_dump(const char *prefix, struct qdma_request *req,
			bool dump_cb);
void qdma_request_cancel_done(struct qdma_descq *descq,
			struct qdma_request *req);


#endif /* ifndef __QDMA_REQUEST_H__ */
