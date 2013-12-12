/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 2.0.4
 *
 * Do not make changes to this file unless you know what you are doing--modify
 * the SWIG interface file instead.
 * ----------------------------------------------------------------------------- */

package org.doubango.tinyWRAP;

public class MsrpMessage {
  private long swigCPtr;
  protected boolean swigCMemOwn;

  public MsrpMessage(long cPtr, boolean cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = cPtr;
  }

  public static long getCPtr(MsrpMessage obj) {
    return (obj == null) ? 0 : obj.swigCPtr;
  }

  protected void finalize() {
    delete();
  }

  public synchronized void delete() {
    if (swigCPtr != 0) {
      if (swigCMemOwn) {
        swigCMemOwn = false;
        tinyWRAPJNI.delete_MsrpMessage(swigCPtr);
      }
      swigCPtr = 0;
    }
  }

  public MsrpMessage() {
    this(tinyWRAPJNI.new_MsrpMessage(), true);
  }

  public boolean isRequest() {
    return tinyWRAPJNI.MsrpMessage_isRequest(swigCPtr, this);
  }

  public short getCode() {
    return tinyWRAPJNI.MsrpMessage_getCode(swigCPtr, this);
  }

  public String getPhrase() {
    return tinyWRAPJNI.MsrpMessage_getPhrase(swigCPtr, this);
  }

  public tmsrp_request_type_t getRequestType() {
    return tmsrp_request_type_t.swigToEnum(tinyWRAPJNI.MsrpMessage_getRequestType(swigCPtr, this));
  }

  public void getByteRange(long[] arg0, long[] arg1, long[] arg2) {
    tinyWRAPJNI.MsrpMessage_getByteRange(swigCPtr, this, arg0, arg1, arg2);
  }

  public boolean isLastChunck() {
    return tinyWRAPJNI.MsrpMessage_isLastChunck(swigCPtr, this);
  }

  public boolean isFirstChunck() {
    return tinyWRAPJNI.MsrpMessage_isFirstChunck(swigCPtr, this);
  }

  public boolean isSuccessReport() {
    return tinyWRAPJNI.MsrpMessage_isSuccessReport(swigCPtr, this);
  }

  public String getMsrpHeaderValue(String name) {
    return tinyWRAPJNI.MsrpMessage_getMsrpHeaderValue(swigCPtr, this, name);
  }

  public String getMsrpHeaderParamValue(String name, String param) {
    return tinyWRAPJNI.MsrpMessage_getMsrpHeaderParamValue(swigCPtr, this, name, param);
  }

  public long getMsrpContentLength() {
    return tinyWRAPJNI.MsrpMessage_getMsrpContentLength(swigCPtr, this);
  }

  public long getMsrpContent(java.nio.ByteBuffer output, long maxsize) {
    return tinyWRAPJNI.MsrpMessage_getMsrpContent(swigCPtr, this, output, maxsize);
  }

}
