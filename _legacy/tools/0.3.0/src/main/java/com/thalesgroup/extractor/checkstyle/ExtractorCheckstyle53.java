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

package com.thalesgroup.extractor.checkstyle;

import java.io.File;

import com.google.inject.Inject;
import com.google.inject.name.Named;
import com.thalesgroup.extractor.core.AbstractExtractor;
import com.thalesgroup.dtkit.util.converter.ConversionService;

public class ExtractorCheckstyle53 extends AbstractExtractor {

	private String inputFile;
	
	@SuppressWarnings("unused")
	@Inject
	private void init(@Named ("inputFile") String inputFile,
					  @Named ("outFilePath") String outFilePath){
		this.inputFile = inputFile;
		this.outFilePath = outFilePath;
	}
	
	@Override
	public void extract() {
		CoreCheckstyle53 core = new CoreCheckstyle53();
		core.extract(new File(inputFile), new File(outFilePath));
	}

	@Override
	public String getToolName() {
		return "Checkstyle";
	}

	@Override
	public String getVersion() {
		return "5.3";
	}

	@Override
	public boolean isDefaultVersion() {
		return true;
	}

}
