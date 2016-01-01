package com.thalesgroup.extractor.checkstyle;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.Formatter;

import javax.xml.transform.stream.StreamSource;

public class CoreCheckstyle53 {

	public void extract(File inputFile, File outFile){
		if( inputFile.exists() ){
			// call to importer
		}
		else{
			Formatter output;
			try {
				output = new Formatter( outFile );
				output.format((new StreamSource(this.getClass().getResourceAsStream("checkstyle53/rules.xml")).toString()));
			} catch (FileNotFoundException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			
		}
	}
}
