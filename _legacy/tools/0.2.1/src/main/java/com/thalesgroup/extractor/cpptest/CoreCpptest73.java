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
import java.io.FileNotFoundException;
import java.io.UnsupportedEncodingException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Scanner;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.thalesgroup.extractor.core.OnlyHtml;
import com.thalesgroup.extractor.core.Rule;
import com.thalesgroup.extractor.core.SonarRulesFormatter;

public class CoreCpptest73 {
	
	private  static final String a_str_Markup[]={
		"<HTML>",
		"<HEAD>",
		"<TITLE>",
		"</TITLE>",
		"</HEAD>",
		"<STRONG>",
		"</STRONG>",
		"<PRE>",
		"</PRE>"
	};

	private  static final String str_toolmarkup = 
	"<!-- CPPTEST 7.3 -->\n";

	public boolean validateInputFile(File inputFile){
		boolean isValid = true;
		boolean a_b_Step[] = new boolean[a_str_Markup.length];
		int i_step = 0, i_step_max = a_str_Markup.length;
		
		for(int i=0; i<a_str_Markup.length; i++){ // array init
			a_b_Step[i] = false;
		}
		try {
			List< String > lines = readLines(inputFile);
			ListIterator< String > iterator = lines.listIterator();
			while ( iterator.hasNext() ) {             // loop on each field
				String line = new String(iterator.next().getBytes(), "UTF-8");
				if( i_step == i_step_max){ // avoid array overflow
					break;}
				String markup = new String(a_str_Markup[i_step]);
				String markup_UTF_8 = new String(markup.getBytes(), "UTF-8");
				if( 0 == line.compareTo(markup_UTF_8)){ // step n
					a_b_Step[i_step++] = true; 
				}
			}
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (UnsupportedEncodingException e) {
			e.printStackTrace();
		}
		for(int i=0; i<i_step_max; i++){ 
			if(!a_b_Step[i]) 
			isValid &= a_b_Step[i];
		}
		return isValid;
	}
	
	public void extract(File inputDir, File outFile){
		SonarRulesFormatter sonar = new SonarRulesFormatter(CoreCpptest73.str_toolmarkup);
		if(inputDir.isDirectory()){
			Map< String, Rule > mapRules = new LinkedHashMap< String, Rule >();
			File[] lst_files = inputDir.listFiles(new OnlyHtml());
			Arrays.sort(lst_files);
			for (int i=0; i<lst_files.length; i++){
				if(validateInputFile(lst_files[i])){
					Rule rule = extractRule(lst_files[i]);
					if(rule != null && !rule.id.equals("")){
						mapRules.put(rule.id, rule);
					}
				}else{
					System.out.println(lst_files[i]+" is not a valid file!");
				}
			}
			sonar.format(mapRules, outFile);
			System.out.println("Total rules: " + mapRules.size());
		}
	}
	
	private Rule extractRule(File inputFile){
		Rule rule = new Rule();
		String line = null;
		try {
			List< String > lines = readLines(inputFile);
			ListIterator< String > iterator = lines.listIterator();
			boolean b_FoundId = false, b_FoundDesc = false;
			while ( iterator.hasNext() ) {             // loop on each field
				line = new String(iterator.next());
				if(!b_FoundId){
					b_FoundId = extractRuleInfos(line, rule);
				}
				else if(!b_FoundDesc){
					b_FoundDesc = extractRuleDesc(line, iterator, rule);
				}
				else{
					break;
				}
			}
		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return rule;
	}
	
	private boolean extractRuleInfos(String line, Rule rule){
		boolean b_FoundId = false;
		Pattern pat = Pattern.compile("^(.*)(\\[[^\\s]{3,}\\])$");
		if(rule!=null && rule.id.equals("")){
			if(!line.contains("<")){                   // test mark-up
				Matcher m = pat.matcher(line);         // search id
				if (m.find()) {
			    	b_FoundId = true;
			    	String Id = new String();
			    	Id = m.group(2).substring(1, m.group(2).length()-1);
			    	int length = Id.length();
			    	rule.id = Id.substring(0, length-2);
			    	rule.name = replaceHTMLcodes(m.group(1).trim());
			    	rule.priority = getPriority(Id.substring(length-1));
			    	rule.configKey = Id;
			    	rule.category = "Reliability";
			    }
			}
		}
		return b_FoundId;
	}
	
	private boolean extractRuleDesc(String line, ListIterator< String > iterator, Rule rule){
		boolean b_FoundDesc = false;
		if(line.equals("<PRE>")){
			b_FoundDesc = true;
			rule.description = "";
			while(iterator.hasNext()){
				String next = iterator.next();
				if(next.equals("</PRE>")){
					break;
				}
				if(!next.equals("")){
					rule.description += replaceHTMLcodes(next) + "\n";
				}
			}
			rule.description = new String(rule.name); // sorry, in sonar JDBC fail loading string above
		}
		return b_FoundDesc;
	}
	
	private String getPriority(String priority){
		String prio = new String("INFO");
		Map<String, String> priorities = new HashMap<String, String>(); 
		priorities.put("1", "BLOCKER" );
		priorities.put("2", "CRITICAL" );
		priorities.put("3", "MAJOR" );
		priorities.put("4", "MINOR" );
		priorities.put("5", "INFO" );
		if(priorities.containsKey(priority)){
			prio = priorities.get(priority);
		}
		return prio;
	}
	
	private String replaceHTMLcodes(String str){
		String cleanstr = new String();
		cleanstr = str.replace("&amp;", "&");
		cleanstr = cleanstr.replace("&quot;", "\"");
		if(cleanstr.contains("&lt;%")){
			cleanstr = cleanstr.replace("&lt;%", "'<'%");
		}
		cleanstr = cleanstr.replace("&lt;", "<");
		cleanstr = cleanstr.replace("&gt;", ">");
		return cleanstr;
	}
	
	private List< String > readLines(File InputFile) throws FileNotFoundException {
		Scanner scanner = new Scanner(InputFile);
		List< String > lines = new LinkedList< String >();
		// loop on each field
		while (scanner.hasNextLine()) {
			String line = new String(scanner.nextLine());
			lines.add(line);
		}
		scanner.close();
		return lines;
	}
}
