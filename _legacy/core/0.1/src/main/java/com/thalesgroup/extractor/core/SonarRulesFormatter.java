package com.thalesgroup.extractor.core;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.Formatter;
import java.util.Map;
import java.util.Map.Entry;

public class SonarRulesFormatter {

	public  static final String str_filehead = 
		"<?xml version=\"1.0\"?>\n"+
		"<rules>\n";
	
	public  static final String str_filetail =
		"</rules>";
	
	public  static final String str_violformat = 
	"<rule key=\"%s\" priority=\"%s\">\n"+
	"\t<name><![CDATA[%s]]></name>\n"+
	"\t<configKey><![CDATA[%s]]></configKey>\n"+
	"\t<category name=\"%s\"/>\n"+
	"\t<description><![CDATA[%s]]></description>\n"+
	"</rule>\n";
	
	public  String str_toolmarkup = "";
	
	public SonarRulesFormatter(){
	}
	
	public SonarRulesFormatter(String str_toolmarkup){
		this();
		this.str_toolmarkup = str_toolmarkup;
	}
	
	public void format(Map< String, Rule > mapRules, File outFile){
		Formatter output;
		try {
			output = new Formatter( outFile );
			output.format(str_filehead+str_toolmarkup);
			for( Entry<String, Rule> entry  : mapRules.entrySet()){
				output.format( str_violformat, entry.getKey(),
				entry.getValue().priority, entry.getValue().name, entry.getValue().configKey, 
				entry.getValue().category, entry.getValue().description);
			}
			output.format(str_filetail);
			if ( output != null )
				output.close();
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		}
	}
}
