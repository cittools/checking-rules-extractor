/*******************************************************************************
 * Copyright (c) 2011 Thales Corporate Services SAS                             *
 * Author : Aravindan Mahendran                                                 *
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

package com.thalesgroup.extractor.klocwork;

import com.google.inject.Inject;
import com.google.inject.name.Named;
import com.thalesgroup.extractor.core.AbstractExtractor;

public class ExtractorKlocwork92 extends AbstractExtractor{
	
	private String inputHTMLDir;
	private String inputXMLDir;
	private String configFile;
	
	@SuppressWarnings("unused")
	@Inject
	private void init(@Named ("inputHTMLDir") String inputHTMLDir, @Named ("inputXMLDir") String inputXMLDir, @Named ("configFile") String configFile, @Named ("outFilePath") String outFilePath){
		this.inputHTMLDir = inputHTMLDir;
		this.inputXMLDir = inputXMLDir;
		this.configFile = configFile;
		this.outFilePath = outFilePath;
	}

	@Override
	public void extract() {
		CoreKlocwork92 core = new CoreKlocwork92();
		core.extract(inputHTMLDir, inputXMLDir, configFile, outFilePath);
		System.out.println("Extraction finished\n");
	}

	@Override
	public String getToolName() {
		return "Klocwork";
	}

	@Override
	public String getVersion() {
		return "9.2";
	}

	@Override
	public boolean isDefaultVersion() {
		return true;
	}

}
