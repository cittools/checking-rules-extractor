package com.thalesgroup.extractor.junit;

import org.junit.Test;

import com.thalesgroup.extractor.cpptest.ExtractorCpptest73;

public class Cpptest73Test extends AbstractTest {

	@Test
    public void testcase1() throws Exception {
        convertAndValidate( ExtractorCpptest73.class, "cpptest/cpptest.ini");
    }
}
