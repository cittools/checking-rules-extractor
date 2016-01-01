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

package com.thalesgroup.extractor.cpptest;

import java.io.File;

import com.google.inject.Inject;
import com.google.inject.name.Named;
import com.thalesgroup.extractor.core.AbstractExtractor;


public class ExtractorCpptest73 extends AbstractExtractor{
	
	private String inputDir;
	
	@SuppressWarnings("unused")
	@Inject
	private void init(@Named ("inputDir") String inputDir, @Named ("outFilePath") String outFilePath){
		this.inputDir = inputDir;
		this.outFilePath = outFilePath;
	}
			
	@Override
	public void extract() {
		CoreCpptest73 core = new CoreCpptest73();
		core.extract(new File(inputDir), new File(outFilePath));
		
		System.out.println("Extraction finished!.\n");
	}

	@Override
	public String getToolName() {
		// TODO Auto-generated method stub
		return "Cpptest";
	}

	@Override
	public String getVersion() {
		// TODO Auto-generated method stub
		return "7.3";
	}

	@Override
	public boolean isDefaultVersion() {
		// TODO Auto-generated method stub
		return true;
	}



	

}

