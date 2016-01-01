package com.thalesgroup.extractor.junit;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Properties;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.name.Names;
import com.thalesgroup.extractor.core.AbstractExtractor;

public class AbstractTest {

    public void convertAndValidate(Class<? extends AbstractExtractor> extractorClassType, String testpropertiesPath) throws Exception {

    	try {
    		final String propertiesPath = testpropertiesPath;
	    	final Class<? extends AbstractExtractor> extractor = extractorClassType;
			
			if( extractor == null ){
				throw new Exception("Extractor for tool name not found!");
			}
			
	    	Injector injector = Guice.createInjector(new AbstractModule(){
	
				@Override
				protected void configure() {
										
					try {						
						Properties properties = getProperties(propertiesPath);
						
						for (String key: properties.stringPropertyNames()){							
							bind(String.class).annotatedWith(Names.named(key)).toInstance(properties.getProperty(key));
						}
						bind(AbstractExtractor.class).to(extractor);
					} catch (Exception e) {
						e.printStackTrace();
					}	
				}
								
			});
			 
			AbstractExtractor c = injector.getInstance(extractor);
			
			c.extract();
		
	    } catch (Exception e) {
			e.printStackTrace();
		}
    }
    
	private Properties getProperties(String propertiesPath){
		
		Properties properties = null;
		FileInputStream fis = null;
		
		try {
			fis = new FileInputStream(new File(propertiesPath));
			properties = new Properties();
			properties.load(fis);
			
		} catch (FileNotFoundException e1) {
			e1.printStackTrace();						
		} catch (IOException e) {
			e.printStackTrace();
		}
		finally{
		 if (fis!=null){
			try {
				fis.close();
			} catch (IOException e) {
				e.printStackTrace();
			}
		 }
		}				
		return properties;
		
	}
}
