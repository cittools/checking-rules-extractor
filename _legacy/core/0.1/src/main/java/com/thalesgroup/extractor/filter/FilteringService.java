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

import java.util.Iterator;
import java.util.ServiceLoader;

import com.thalesgroup.extractor.client.ClientContext;
import com.thalesgroup.extractor.core.AbstractExtractor;

public class FilteringService {
	
	public ClientContext cli;
	
	/**
	 * Constructor by default
	 *
	 */
	public FilteringService(){
		this.cli = null;
	}
	
	/**
	 * Constuctor initialize the client context.
	 * @param client context to initialize.
	 */
	public FilteringService(ClientContext client){
		this();
		this.cli = client;
	}
	
	//public Class 
	public Class<? extends AbstractExtractor> getExtractClass() throws Exception {
		
		String pluginPath = this.cli.context.get(ClientContext.PLUGIN_PATH);
		Class<? extends AbstractExtractor> extractor = null;
		PluginLoaderService loader = new PluginLoaderService(pluginPath);
		ServiceLoader<AbstractExtractor> extractors = loader.loadAllPlugins();
				
		Iterator<AbstractExtractor> it = extractors.iterator();
		while(it.hasNext()){
			AbstractExtractor currentExtractor = it.next();
			
			if(cli.context.get(ClientContext.TOOL_NAME).equalsIgnoreCase(currentExtractor.getToolName())){
				// check tool version
				if(cli.context.get(ClientContext.TOOL_VERSION) == null){
					if(currentExtractor.isDefaultVersion()){
						extractor = currentExtractor.getClass();
						break;
					}
				}
				else if(cli.context.get(ClientContext.TOOL_VERSION).equals(currentExtractor.getVersion())){
					extractor = currentExtractor.getClass();
					break;
				}
			}
		}
		return extractor;
	}
}
