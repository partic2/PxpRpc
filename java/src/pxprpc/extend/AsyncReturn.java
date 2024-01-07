package pxprpc.extend;

public interface AsyncReturn<T> {
    void resolve(T r);
    void reject(Exception ex);
}
