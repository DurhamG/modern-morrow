package io.github.durhamg.modernmorrow;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class DataImportTest {
    @Test
    public void sizeShouldUseGigabytesFromOneGigabyte() {
        assertEquals("1.0 GB", DataImport.size(1L << 30));
        assertEquals("512 MB", DataImport.size(512L << 20));
    }
}
