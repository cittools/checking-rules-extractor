package com.thalesgroup.extractor.junit;

import org.junit.Test;

import com.thalesgroup.extractor.checkstyle.ExtractorCheckstyle53;

public class Checkstyle53Test extends AbstractTest {

	@Test
    public void testcase1() throws Exception {
        convertAndValidate( ExtractorCheckstyle53.class, "checkstyle/checkstyle.ini");
    }
}
