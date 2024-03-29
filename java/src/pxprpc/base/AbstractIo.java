package pxprpc.base;

import java.io.IOException;
import java.nio.ByteBuffer;

public interface AbstractIo {
    void send(ByteBuffer[] buffs) throws IOException;
    // the last buffer must be null to be set the last buffer for the rest data.
    void receive(ByteBuffer[] buffs) throws IOException;
    void close();
}
