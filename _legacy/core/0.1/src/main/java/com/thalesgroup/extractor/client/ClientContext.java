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

package com.thalesgroup.extractor.client;

import java.util.HashMap;
import java.util.Map;

public class ClientContext {
	
	public static final String TOOL_NAME 		= "toolName";
	public static final String TOOL_VERSION 	= "version";
	public static final String PROPERTIES_PATH 	= "propertiesPath";
	public static final String PLUGIN_PATH 		= "pluginPath";
	
	public String propNames[] = { TOOL_NAME, TOOL_VERSION, PROPERTIES_PATH, PLUGIN_PATH };
		
	public Map<String, String> context;
	
	ClientContext(){
				
		context = new HashMap<String, String>();
		
		for(int i=0; i<propNames.length; i++ ){
			
			context.put(propNames[i], null);
		}
	}

}
