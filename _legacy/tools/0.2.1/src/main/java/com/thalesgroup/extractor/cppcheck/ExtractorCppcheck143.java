package com.thalesgroup.extractor.cppcheck;

import java.io.File;

import com.google.inject.Inject;
import com.google.inject.name.Named;
import com.thalesgroup.extractor.core.AbstractExtractor;

public class ExtractorCppcheck143 extends AbstractExtractor {

	private String inputDir;

	@SuppressWarnings("unused")
	@Inject
	private void init(@Named("inputDir") String inputDir, @Named ("outFilePath") String outFilePath){
		this.inputDir = inputDir;
		this.outFilePath = outFilePath;
	}

	@Override
	public void extract() {

		CoreCppcheck143 core = new CoreCppcheck143();
		core.extract(new File (inputDir), new File (outFilePath));
	}

	@Override
	public String getToolName() {
		return "Cppcheck";
	}

	@Override
	public String getVersion() {
		return "1.43";
	}

	@Override
	public boolean isDefaultVersion() {
		return true;
	}

}
