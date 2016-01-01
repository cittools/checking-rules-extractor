package com.thalesgroup.extractor.junit;

import org.junit.Test;

import com.thalesgroup.extractor.logiscope.ExtractorLogiscope63;

public class Logiscope63Test extends AbstractTest {

	@Test
    public void testcase1() throws Exception {
        convertAndValidate( ExtractorLogiscope63.class, "logiscope/logiscope.ini");
    }
}
