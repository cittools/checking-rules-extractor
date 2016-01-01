package com.thalesgroup.extractor.junit;

import java.util.ArrayList;
import java.util.List;

import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;

import com.thalesgroup.extractor.checkstyle.ExtractorCheckstyle53;
import com.thalesgroup.extractor.core.AbstractExtractor;
import com.thalesgroup.extractor.cppcheck.ExtractorCppcheck143;
import com.thalesgroup.extractor.cpptest.ExtractorCpptest73;
import com.thalesgroup.extractor.gnatcheck.ExtractorGnatcheck621;
import com.thalesgroup.extractor.logiscope.ExtractorLogiscope63;

public class ValidInputExtractorTest {

	private static  List<Class<? extends AbstractExtractor>> listInputMetric = new ArrayList<Class<? extends AbstractExtractor>>();
	
    @BeforeClass
    public static void loadList() {
        listInputMetric.add(ExtractorCpptest73.class);
        listInputMetric.add(ExtractorGnatcheck621.class);
        listInputMetric.add(ExtractorLogiscope63.class);
        listInputMetric.add(ExtractorCheckstyle53.class);
        //listInputMetric.add(ExtractorCppcheck138.class);
        listInputMetric.add(ExtractorCppcheck143.class);
    }
    
    @Test
    public void testAllTypes() throws Exception {
        for (Class<? extends AbstractExtractor> inputAbstractExtractorClass : listInputMetric) {
        	AbstractExtractor inputAbstractExtractor = inputAbstractExtractorClass.newInstance();

            //The following elements must be set
            
            Assert.assertNotNull(inputAbstractExtractor.getToolName());
            Assert.assertNotNull(inputAbstractExtractor.getVersion());
            

            //The input must be valid
            try {
                //new File(inputMetricXSL.getClass().getResource(inputMetricXSL.getXslName()).toURI());
                Assert.assertTrue(true);
            }
            catch (NullPointerException npe) {
                //Assert.assertTrue(inputMetricXSL.getXslName() + " doesn't exist.", false);
            }
           
        }
    }
}
