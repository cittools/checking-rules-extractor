/*******************************************************************************
 * Copyright (c) 2010 Thales Corporate Services SAS                             *
 * Author : Joel Forner                                                         *
 *                                                                              *
 * Permission is hereby granted, free of charge, to any person obtaining a copy *
 * of this software and associated documentation files (the "Software"), to deal*
 * in the Software without restriction, including without limitation the rights *
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell    *
 * copies of the Software, and to permit persons to whom the Software is        *
 * furnished to do so, subject to the following conditions:                     *
 *                                                                              *
 * The above copyright notice and this permission notice shall be included in   *
 * all copies or substantial portions of the Software.                          *
 *                                                                              *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR   *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,     *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE  *
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER       *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,*
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN    *
 * THE SOFTWARE.                                                                *
 *******************************************************************************/

package com.thalesgroup.extractor.core;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Properties;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.name.Names;
import com.thalesgroup.extractor.client.ClientContext;
import com.thalesgroup.extractor.filter.FilteringService;

public class ExtractionService {

	public ClientContext cli;
	
	/**
	 * Constructor by default
	 *
	 */
	public ExtractionService(){
		this.cli = null;
	}
	
	/**
	 * Constuctor initialize the client context.
	 * @param client context to initialize.
	 */
	public ExtractionService(ClientContext client){
		this();
		this.cli = client;
	}
	
	public void getExtraction(){
		
		FilteringService filter = new FilteringService(cli);
		
		try {
			
			final Class<? extends AbstractExtractor> extractor = filter.getExtractClass();
			
			if( extractor == null ){
				throw new Exception("Extractor for tool name not found!");
			}
			
			Injector injector = Guice.createInjector(new AbstractModule(){

				@Override
				protected void configure() {
										
					try {						
						Properties properties = getProperties();
						
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
	
	private Properties getProperties(){
		Properties properties = null;
		String propertiesPath = cli.context.get(ClientContext.PROPERTIES_PATH);
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
