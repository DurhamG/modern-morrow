package io.github.durhamg.modernmorrow;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class DataFilesTest {
    @Test
    public void sizeShouldUseGigabytesFromOneGigabyte() {
        assertEquals("1.0 GB", DataFiles.size(1L << 30));
        assertEquals("512 MB", DataFiles.size(512L << 20));
    }
}
