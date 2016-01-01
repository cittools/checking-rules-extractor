/*******************************************************************************
 * Copyright (c) 2010 Thales Corporate Services SAS                             *
 * Author : Joel Forner, Aravindan Mahendran                                    *
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

package com.thalesgroup.extractor.logiscope;

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

import com.thalesgroup.extractor.core.Rule;
import com.thalesgroup.extractor.core.SonarRulesFormatter;

public class CoreLogiscope63 {

	private  static final String a_str_Markup[]={
		".NAME",
		".SEVERITY"
	};
	
	private static final int NAME_LENGTH = 192;

	private  static final String str_toolmarkup = 
	"<!-- LOGISCOPE RULECHECKER 6.3 -->\n";

	/**
	 * Check that the given file corresponds to a Logiscope rule.
	 * @param inputFile Logiscope rule (.rl or .rst file)
	 * @return true if the file is a Logiscope rule, false otherwise
	 */
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
				if( line.startsWith(markup_UTF_8)){ // step n
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
	
	/**
	 * The method to call in other classes to extract rules.
	 * @param inputDir The directory containing the Logiscope rules
	 * @param outFile The file generated and containing the Logiscope rules
	 */
	public void extract(File inputDir, File outFile){
		Map< String, Rule > mapRules = extract(inputDir);
		if( mapRules != null ){
			SonarRulesFormatter sonar = new SonarRulesFormatter(CoreLogiscope63.str_toolmarkup);
			sonar.format(mapRules, outFile);
		}
	}
	
	/**
	 * Converting the Logiscope default priority into Sonar priority
	 * @param priority Logiscope priority
	 * @return Sonar priority corresponding to the Logiscope priority ("INFO" if the Sonar priority is unknown)
	 */
	private String getPriority(String priority){
		String prio = new String("INFO");
		Map<String, String> priorities = new HashMap<String, String>(); 
		priorities.put("Required", "BLOCKER" );
		priorities.put("Advisory", "CRITICAL" );
		priorities.put("3", "MAJOR" );
		priorities.put("4", "MINOR" );
		priorities.put("5", "INFO" );
		if(priorities.containsKey(priority)){
			prio = priorities.get(priority);
		}
		return prio;
	}
	
	/**
	 * Return a list of String containing the lines of the given input file.
	 * @param inputFile The file to read and analyse
	 * @return A list of String containing the lines of the given file
	 * @throws FileNotFoundException
	 */
	private List< String > readLines(File inputFile) throws FileNotFoundException {
		Scanner scanner = new Scanner(inputFile);
		List< String > lines = new LinkedList< String >();
		// loop on each field
		while (scanner.hasNextLine()) {
			String line = new String(scanner.nextLine());
			lines.add(line);
		}
		scanner.close();
		return lines;
	}
	
	/**
	 * Return a Map containing the different rules extracted from the given folder.
	 * @param inputDir The directory containing the Logiscope rules
	 * @return The map containing the different rules
	 */
	private Map< String, Rule > extract(File inputDir){
		Map< String, Rule > mapRules = null;
		if(inputDir.isDirectory()){
			mapRules = new LinkedHashMap< String, Rule >();
			File[] lst_files = inputDir.listFiles(new OnlyRules());
			Arrays.sort(lst_files);
			for (int i=0; i<lst_files.length; i++){
				if(validateInputFile(lst_files[i])){
					Rule rule = extractRule(lst_files[i]);
					if(rule != null && !rule.id.equals("")){
						mapRules.put(rule.id, rule);
					}
				}else {
					System.out.println(lst_files[i]+" is not a valid file!");
				}
			}
		}
		return mapRules;
	}
	
	/**
	 * Extract the rule from the given Logiscope rule.
	 * @param inputFile The Logiscope rule
	 * @return The Rule extracted from the given file
	 */
	private Rule extractRule(File inputFile){
		Rule r = new Rule();
		try {
			List< String > lines = readLines(inputFile);
			boolean stdDescription = false;
			boolean rlDescription = false;
			String filename = inputFile.getName();
			r.id = filename.substring(0, filename.lastIndexOf("."));
			r.configKey = r.id;
	    	r.category = "Reliability";
	    	r.name="";
			for (String line : lines){
				if (stdDescription){
					
					if (!line.isEmpty()&& !line.startsWith("---") && !line.startsWith("Definition:")){
						r.name+=" "+line;
					}
				}
				
				//In .rl files, most of the time, the first line after the ".TITLE Description" Markup is enough to understand the violation...
				else if (rlDescription){
					r.name+=" "+line;
					
					//...But in some .rl files, we have to copy more lines
					if (!line.endsWith(":") && !line.startsWith("-")){
						rlDescription=false;
					}
					
				}
				
				if (line.startsWith(".NAME ")){
					r.description = line.substring(6);
				}
				else if (line.startsWith(".SEVERITY ")){
					r.priority = getPriority(line.substring(10));
				}
				else if (line.startsWith(".DESCRIPTION")){
					stdDescription = true;
				}
				else if (line.startsWith(".TITLE Description")){
					rlDescription = true;
				}
			}
			
			if (stdDescription /*|| rlDescription*/){
				
				//Trying to get characters until ". " is met
				if (r.name.indexOf(". ") != -1){
					r.name = r.name.substring(0, r.name.indexOf(". "))/*.trim()*/;
				}
				
				//Trying to get characters until "." is met
				else {
					r.name = r.name.substring(0, r.name.indexOf("."))/*.trim()*/;
				}
				
				//Last problem : sometimes, the above verification is not enough strengthful. So we have to check that there isn't more than the description in the string
				if (r.name.indexOf(".SEVERITY") != -1){
					r.name = r.name.substring(0, r.name.indexOf(".SEVERITY"));
				}
			}
			r.name = r.name.trim();
			
			if (r.name.length() >= NAME_LENGTH){
				r.name = r.name.substring(0, NAME_LENGTH-1);
			}
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		}
		
		
		return r;
	}
	
}
