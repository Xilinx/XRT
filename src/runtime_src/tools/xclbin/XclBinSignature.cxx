/**
 * Copyright (C) 2019 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "XclBinSignature.h"

#include <iostream>
#include <vector>

#ifndef _WIN32
  #include <openssl/cms.h>
  #include <openssl/pem.h>
#endif

#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;

// Status structure
typedef struct {
  bool bIsXclImage;
  bool bIsSigned;
  uint64_t actualFileSize;
  uint64_t headerFileLength;
  int32_t signatureLength;
} XclBinImageStats;


static void
getXclBinStats(const std::string& _xclBinFile,
               XclBinImageStats& _xclBinImageStats) {
  // Initialize return values
  _xclBinImageStats = { 0 };

  // Error checks
  if (_xclBinFile.empty()) {
    std::string errMsg = "ERROR: Missing xclbin file name to read from.";
    throw std::runtime_error(errMsg);
  }

  // Open the file for consumption
  XUtil::TRACE("Reading xclbin binary file: " + _xclBinFile);
  std::fstream ifXclBin;
  ifXclBin.open(_xclBinFile, std::ifstream::in | std::ifstream::binary);
  if (!ifXclBin.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + _xclBinFile;
    throw std::runtime_error(errMsg);
  }

  // Determine File Size
  ifXclBin.seekg(0, ifXclBin.end);
  _xclBinImageStats.actualFileSize = ifXclBin.tellg();

  // Read in the header buffer
  axlf xclBinHeader;
  const unsigned int expectBufferSize = sizeof(axlf);

  ifXclBin.seekg(0);
  ifXclBin.read((char*)&xclBinHeader, sizeof(axlf));

  if (ifXclBin.gcount() != expectBufferSize) {
    std::string errMsg = "ERROR: xclbin file is smaller than the header size.";
    throw std::runtime_error(errMsg);
  }

  std::string sMagicValue = XUtil::format("%s", xclBinHeader.m_magic).c_str();
  if (sMagicValue.compare("xclbin2") != 0) {
    std::string errMsg = XUtil::format("ERROR: The XCLBIN appears to be corrupted.  Expected magic value: 'xclbin2', actual: '%s'", sMagicValue.c_str());
    throw std::runtime_error(errMsg);
  }

  // We know it is an xclbin archive
  _xclBinImageStats.bIsXclImage = true;

  // Get signature information
  if (xclBinHeader.m_signature_length != -1) {
    _xclBinImageStats.bIsSigned = true;
    _xclBinImageStats.signatureLength = xclBinHeader.m_signature_length;

    if (xclBinHeader.m_signature_length < -1) {
      throw std::runtime_error("ERROR: xclbin recorded signature length is corrupted.");
    }
  }

  // Get header file length
  _xclBinImageStats.headerFileLength = xclBinHeader.m_header.m_length;

  // Now do some simple DRC checks
  uint64_t expectedFileSize = _xclBinImageStats.headerFileLength;

  if (_xclBinImageStats.bIsSigned) {
    expectedFileSize += _xclBinImageStats.signatureLength;
  }

  if (expectedFileSize != _xclBinImageStats.actualFileSize) {
    std::string errMsg = XUtil::format("ERROR: Expected files size (0x%lx) does not match actual (0x%lx)", expectedFileSize, _xclBinImageStats.actualFileSize);
    throw std::runtime_error(errMsg);
  }

  // We are done
  ifXclBin.close();
}


void signXclBinImage(const std::string& _fileOnDisk,
                     const std::string& _sPrivateKey,
                     const std::string& _sCertificate)
// Equivalent openssl command:
//   openssl cms -md sha512 -nocerts -noattr -sign -signer certificate.cer -inkey private.key -binary -in u50.dts -outform der -out signature.openssl
#ifdef _WIN32
{
  throw std::runtime_error("ERROR: signXclBinImage not implemented on windows");
}
#else
{
  std::cout << "----------------------------------------------------------------------" << std::endl;
  std::cout << "Signing the archive file: '" + _fileOnDisk + "'" << std::endl;
  std::cout << "        Private key file: '" + _sPrivateKey + "'" << std::endl;
  std::cout << "        Certificate file: '" + _sCertificate + "'" << std::endl;


  XUtil::TRACE("SignXclBinImage");
  XUtil::TRACE("File On Disk: '" + _fileOnDisk + "'");
  XUtil::TRACE("Private Key: '" + _sPrivateKey + "'");
  XUtil::TRACE("Certificate: '" + _sCertificate + "'");

  // -- Do some DRC checks on the image
  // Is the image on disk
  XclBinImageStats xclBinStats = { 0 };
  getXclBinStats(_fileOnDisk, xclBinStats);

  if (xclBinStats.bIsSigned == true) {
    throw std::runtime_error("ERROR: Xclbin image is already signed. File: '" + _fileOnDisk + "'");
  }

  // *** Calculate the signature **

  std::cout << "Calculating signature..." << std::endl;

  // -- Have openssl point to the xclbin image on disk
  BIO* bmRead = BIO_new_file(_fileOnDisk.c_str(), "rb");

  if (bmRead == nullptr) {
    throw std::runtime_error("ERROR: File missing: '" + _fileOnDisk + "'");
  }

  // -- Read the private key --
  BIO* bmPrivateKey = BIO_new_file(_sPrivateKey.c_str(), "rb");
  if (bmPrivateKey == nullptr) {
    throw std::runtime_error("ERROR: File missing: '" + _sPrivateKey + "'");
  }

  EVP_PKEY* privateKey = PEM_read_bio_PrivateKey(bmPrivateKey, NULL, NULL, NULL);
  if (privateKey == nullptr) {
    throw std::runtime_error("ERROR: Can create private key object.");
  }

  BIO_free(bmPrivateKey);

  // -- Read the certificate --
  BIO* bmCertificate = BIO_new_file(_sCertificate.c_str(), "rb");
  if (bmCertificate == nullptr) {
    throw std::runtime_error("ERROR: File missing: '" + _sCertificate + "'");
  }

  X509* x509 = PEM_read_bio_X509(bmCertificate, NULL, NULL, NULL);
  if (bmCertificate == nullptr) {
    throw std::runtime_error("ERROR: Can create certificate key object.");
  }

  BIO_free(bmCertificate);

  // -- Obtain the digest algorithm --
  OpenSSL_add_all_digests();
  const EVP_MD* digestAlgorithm = EVP_get_digestbyname("sha512");

  if (digestAlgorithm == nullptr) {
    throw std::runtime_error("ERROR: Could not obtain the digest algorithm for 'sha512'");
  }

  // -- Prepare CMS content and signer info --
  CMS_ContentInfo* cmsContentInfo = CMS_sign(NULL, NULL, NULL, NULL,
                                             CMS_NOCERTS | CMS_PARTIAL | CMS_BINARY |
                                             CMS_DETACHED | CMS_STREAM);
  if (cmsContentInfo == nullptr) {
    throw std::runtime_error("ERROR: Could not obtain CMS content info");
  }

  CMS_SignerInfo* cmsSignerInfo = CMS_add1_signer(cmsContentInfo, x509, privateKey, digestAlgorithm,
                                                  CMS_NOCERTS | CMS_BINARY |
                                                  CMS_NOSMIMECAP | CMS_NOATTR);

  if (cmsSignerInfo == nullptr) {
    throw std::runtime_error("ERROR: Could not obtain CMS signer info");
  }

  // -- We are ready to tie it all together --
  if (CMS_final(cmsContentInfo, bmRead, NULL, CMS_NOCERTS | CMS_BINARY) < 0) {
    throw std::runtime_error("ERROR: In finalizing the CMS content.");
  }

  // We are done close the handles
  BIO_free(bmRead);

  // -- Get the signature --
  BIO* bmMem = BIO_new(BIO_s_mem());
  if (i2d_CMS_bio_stream(bmMem, cmsContentInfo, NULL, 0) < 0) {
    throw std::runtime_error("ERROR: Writing to the signature.bin to the in-memory buffer");
  }

  BUF_MEM *bufMem = nullptr;
  BIO_get_mem_ptr(bmMem, &bufMem);
  XUtil::TRACE_BUF("Signature", bufMem->data, bufMem->length);

  // ** Now update the xclbin archive image **

  XUtil::TRACE(XUtil::format("Setting the signature length to: 0x%x", bufMem->length).c_str());
  std::fstream iofXclBin;
  iofXclBin.open(_fileOnDisk, std::ios::in | std::ios::out | std::ios::binary);
  if (!iofXclBin.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading / writing: " + _fileOnDisk;
    throw std::runtime_error(errMsg);
  }

  // First the file signature length
  axlf xclBinHeader = {0};
  iofXclBin.seekg(0);
  iofXclBin.read((char*)&xclBinHeader, sizeof(axlf));
  xclBinHeader.m_signature_length = bufMem->length;

  iofXclBin.seekg(0);
  iofXclBin.write((char*)&xclBinHeader, sizeof(axlf));
  
  // Now add the signature
  iofXclBin.seekg(0, iofXclBin.end);
  iofXclBin.write(bufMem->data, bufMem->length);

  // And we are done
  iofXclBin.close();

  std::cout << "Signature calculated and added successfully to the archive." << std::endl;
  std::cout << "----------------------------------------------------------------------" << std::endl;
}
#endif


void verifyXclBinImage(const std::string& _fileOnDisk,
                       const std::string& _sCertificate)

// Equivalent openssl command:
// openssl smime -verify -in signature.openssl.small -inform DER -content u50.dts -noverify -certfile certificate.cer -binary > /dev/null
#ifdef _WIN32
{
  throw std::runtime_error("ERROR: verifyXclBinImage not implemented on windows");
}
#else
{
  std::cout << "----------------------------------------------------------------------" << std::endl;
  std::cout << "Verifying signature for archive file: '" + _fileOnDisk + "'" << std::endl;
  std::cout << "                    Certificate file: '" + _sCertificate + "'" << std::endl;

  XUtil::TRACE("SignXclBinImage");
  XUtil::TRACE("File On Disk: '" + _fileOnDisk + "'");
  XUtil::TRACE("Certificate: '" + _sCertificate + "'");

  // -- Do some DRC checks on the image
  // Is the image on disk
  XclBinImageStats xclBinStats = { 0 };
  getXclBinStats(_fileOnDisk, xclBinStats);

  if (xclBinStats.bIsSigned == false) {
    throw std::runtime_error("ERROR: Xclbin image is not signed. File: '" + _fileOnDisk + "'");
  }

  // ** Read in the memory image **
  std::cout << "Reading archive file..." << std::endl;

  std::ifstream ifs(_fileOnDisk, std::ios::binary | std::ios::ate);
  std::ifstream::pos_type pos = ifs.tellg();
  std::vector<char> memImage(pos);
  ifs.seekg(0, std::ios::beg);
  ifs.read(memImage.data(), pos);

  // -- Change the signature length to -1 (since this was its signed value)
  axlf *pXclBinHeader = (axlf *) memImage.data();
  pXclBinHeader->m_signature_length = -1;

  // ** Calculate the signature

  std::cout << "Validating signature..." << std::endl;

  BIO *bmImage = BIO_new_mem_buf(memImage.data(), xclBinStats.headerFileLength);
  BIO *bmSignature = BIO_new_mem_buf((char *)(memImage.data() + xclBinStats.headerFileLength), xclBinStats.signatureLength);
  
  // -- Obtain the digest algorithm --
  OpenSSL_add_all_digests();

  // -- Read the certificate --
  BIO* bmCertificate = BIO_new_file(_sCertificate.c_str(), "rb");
  if (bmCertificate == nullptr) {
    throw std::runtime_error("ERROR: File missing: '" + _sCertificate + "'");
  }

  X509* x509 = PEM_read_bio_X509(bmCertificate, NULL, NULL, NULL);
  if (x509 == nullptr) {
    throw std::runtime_error("ERROR: Can create certificate key object.");
  }

  BIO_free(bmCertificate);

  // -- Set up trusted CA certificate store --
  X509_STORE* store = X509_STORE_new();

  if (!X509_STORE_add_cert(store, x509)) {
    throw std::runtime_error("ERROR: Can't add certificate.");
  }

  // -- Read in signature --
  PKCS7* p7 = d2i_PKCS7_bio(bmSignature, NULL);
  if (p7 == NULL) {
    throw std::runtime_error("ERROR: P7 is null.");
  }

  STACK_OF(X509) * ca_stack = sk_X509_new_null();
  sk_X509_push(ca_stack, x509);

  if (!PKCS7_verify(p7, ca_stack, store, bmImage, NULL, PKCS7_DETACHED |  PKCS7_BINARY | PKCS7_NOINTERN)) {
    std::cout << "Signed xclbin archive verification failed" << std::endl;
  } else {
    std::cout << "Signed xclbin archive verification successful" << std::endl;
  }
  std::cout << "----------------------------------------------------------------------" << std::endl;
}
#endif
