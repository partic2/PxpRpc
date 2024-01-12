package pxprpc.extend;

import pxprpc.base.PxpRequest;

public interface AsyncReturn<T> {
    void resolve(T r);
    void reject(Exception ex);
    PxpRequest getRequest();
}
