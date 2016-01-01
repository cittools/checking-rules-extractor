package com.thalesgroup.extractor.junit;

import org.junit.Test;

import com.thalesgroup.extractor.cppcheck.ExtractorCppcheck138;

public class Cppcheck138 extends AbstractTest {

	@Test
    public void testcase1() throws Exception {
        convertAndValidate( ExtractorCppcheck138.class, "cppcheck/cppcheck.ini");
    }
}
