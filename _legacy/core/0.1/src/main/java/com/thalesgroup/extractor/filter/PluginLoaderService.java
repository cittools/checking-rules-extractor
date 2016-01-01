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

package com.thalesgroup.extractor.filter;

import java.io.File;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.ServiceLoader;
import java.util.jar.JarFile;

import com.thalesgroup.extractor.client.ClientContext;
import com.thalesgroup.extractor.core.AbstractExtractor;

public class PluginLoaderService {
	
	private String pluginPath;
	
	
	/**
	 * Constructor by default
	 *
	 */
	public PluginLoaderService(){
		pluginPath = null;
	}
	
	/**
	 * Constuctor initialize the array of jar files to load.
	 * @param files array of jar files to load.
	 */
	public PluginLoaderService(String path){
		this();
		this.pluginPath = path;
	}
	
	/**
	 * Define the path to jar files to load
	 * @param files
	 */
	public void setFiles(String path ){
		this.pluginPath = path;
	}
	
	public ServiceLoader<AbstractExtractor> loadAllPlugins() throws MalformedURLException {
		
		ServiceLoader<AbstractExtractor> extractorsLoader = null;
		
		//Building a new classloader
        try{
        	
		ClassLoader cl = null;
		
		if( (pluginPath != null)  ) {
								
	        URL[] urls = new URL[1];
	                
	        urls[0] = new URL("file:///" + pluginPath);
	        
	        cl = new URLClassLoader(urls, Thread.currentThread().getContextClassLoader());
		}
		else{
			cl = Thread.currentThread().getContextClassLoader();
		}

        // Retrieving all input type
        
        extractorsLoader = ServiceLoader.load(AbstractExtractor.class, cl);
        extractorsLoader.reload();
        }
        catch(Exception e){
        	e.printStackTrace();
        }
		        
		return extractorsLoader;
	}
		
	
}
