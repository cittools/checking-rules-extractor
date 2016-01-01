package com.thalesgroup.extractor.junit;

import org.junit.Test;


import com.thalesgroup.extractor.gnatcheck.ExtractorGnatcheck621;

public class Gnatcheck621Test extends AbstractTest {

	@Test
    public void testcase1() throws Exception {
        convertAndValidate( ExtractorGnatcheck621.class, "gnatcheck/gnatcheck.ini");
    }
}
