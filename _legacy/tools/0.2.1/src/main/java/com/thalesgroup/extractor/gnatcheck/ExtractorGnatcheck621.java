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

package com.thalesgroup.extractor.gnatcheck;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Formatter;
import java.util.Map;
import java.util.Map.Entry;

import com.google.inject.Inject;
import com.google.inject.name.Named;
import com.thalesgroup.extractor.core.AbstractExtractor;


public class ExtractorGnatcheck621 extends AbstractExtractor{

	private String inputFilePath;
	
	@SuppressWarnings("unused")
	@Inject
	private void init(@Named ("inputFilePath") String inputFilePath, @Named ("outFilePath") String outFilePath){
		this.inputFilePath = inputFilePath;
		this.outFilePath = outFilePath;
	}
	
	public  static final String str_filehead = 
		"<?xml version=\"1.0\"?>\n"+
		"<rules>\n"+
		"<!-- GNATCHECK FOR GNATPRO 6.2.1 -->\n";
	
	public  static final String str_filetail =
		"</rules>";
	
	public  static final String str_violformat = 
	"<rule key=\"%s\" priority=\"%s\">\n"+
	"\t<name><![CDATA[%s]]></name>\n"+
	"\t<configKey><![CDATA[%s]]></configKey>\n"+
	"\t<category name=\"Reliability\"/>\n"+
	"\t<description><![CDATA[%s]]></description>\n"+
	"</rule>\n";
	


	@Override
	public String getToolName() {
		return "Gnatcheck";
	}

	@Override
	public String getVersion() {
		return "6.2.1";
	}

	@Override
	public boolean isDefaultVersion() {
		return true;
	}
	
	@Override
	public void extract() {
				
		extract(new File(inputFilePath), new File(outFilePath));
			
		System.out.println("Extraction finished!.\n");

	}

	public void extract(File inputFile, File outFile){
		try {
			ParserGnatcheck parserGnatcheck = new ParserGnatcheck();
			if(parserGnatcheck.validateInputFile(inputFile)){
				Map<String, String> mapRules = parserGnatcheck.getRules(inputFile);
				format(mapRules, outFile);
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	
	private void format(Map<String, String> mapRules, File outFile){
		Formatter output;
		try {
			output = new Formatter( outFile );
			output.format(str_filehead);
			for( Entry<String, String> entry  : mapRules.entrySet()){
				output.format( str_violformat, entry.getValue(), "INFO", entry.getKey(),
						entry.getKey(), entry.getKey());
			}
			output.format(str_filetail);
			if ( output != null )
				output.close();
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		}
	}
}
