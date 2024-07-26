package pxprpc.base;

public class RemoteError extends RuntimeException{
    public RemoteError(String message) {
        super(message);
    }
}
