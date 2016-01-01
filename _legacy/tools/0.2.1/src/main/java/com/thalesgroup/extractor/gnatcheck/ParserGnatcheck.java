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
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Scanner;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class ParserGnatcheck {
	// following is an estimate of the algorithm complexity
	// where r=number of rules, v=number of violations, f=number of source files
	// and n=number of report lines, and we neglect blank lines
	// cmpl=r.v.(v+r+f) ,if we consider r=cste and f=o(v)=o(n) so we have
	// cmpl=o(n^2), and this has been optimized by the use of hash map which 
	// complexity is O(n)
	
	// member data
	private List< String > lines, section1, section3, rules;
	
	private Map< String, Map< String, List<String> > > map; 
	private Map< String, String > mapRules;
	
	public String a_str_Markup[]={
			"Applied rules:",
			"Disabled rules:",
			"Checked argument sources:",
			"-------- Start Section 1 ------------",
			"-------- End Section 1 ------------",
			"-------- Start Section 2 ------------",
			"-------- End Section 2 ------------",
			"-------- Start Section 3 ------------",
			"-------- End Section 3 ------------"
	};
	
	public String a_str_headpat[]={
			// search for: Rule checking report generated 2010.08.31 17:58
			"^Rule\\schecking\\sreport\\sgenerated\\s\\d{4}\\.\\d{2}\\.\\d{2}"
			+"\\s\\d{2}\\:\\d{2}$",
			// search for: by GNATCHECK (built with ASIS 2.0.R for GNAT Pro 6.2.1 (20090115))
			"^by\\sGNATCHECK\\s\\(built\\swith\\sASIS\\s\\d{1}\\.\\d{1}\\.R\\s"
			+"for\\sGNAT\\sPro\\s\\d{1}\\.\\d{1}\\.\\d{1}\\s\\(\\d{8}\\)\\)$"
	};
	
	public String str_filehead = 
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"+
		"<tusar xmlns_xsi=\"http://www.w3.org/2001/XMLSchema-instance\" version=\"1.0\">\n"+
		"    <violations>\n";
	
	public String str_filetail =
		"    </violations>\n"+
		"</tusar>";
	
	public String str_violformat = 
	"            <violation line=\"%s\"\n"+
	"                       message=\"%s\"\n"+
	"                       key=\"%s\"\n"+
	"                       severity=\"info\"/>\n";

	// member functions
	public boolean validateInputFile(File InputFile) throws IOException {
		boolean isValid = false;
		boolean a_b_Step[] = new boolean[a_str_Markup.length];
		int lineNumber = 0, i_step = 0, i_step_max = a_str_Markup.length;
		
		for(int i=0; i<a_str_Markup.length; i++){ // array init
			a_b_Step[i] = false;}
		Scanner scanner = new Scanner(InputFile);
		String line = new String();
		
		while (scanner.hasNextLine()) {         // loop on each field
			line = scanner.nextLine();	
			lineNumber++;
			if( !(isValid = matchHead(lineNumber,line)) )   //process here...
				break;
			if( i_step == i_step_max){ // avoid array overflow
				break;}
			if( line.equals(a_str_Markup[i_step])){ // step n
				a_b_Step[i_step++] = true; 
			}
		}
		scanner.close();
		for(int i=0; i<i_step_max;i++){
			isValid &= a_b_Step[i];
		}
		return isValid;
	}
	
	public boolean validateOutputFile(File OutputFile) {
		return true;
	}
	
	public void convert(File InputFile, File outFile) throws FileNotFoundException {
		Formatter output; // object used to output text to file
		String line, rule;
		String matchedfile = new String("");
		output = new Formatter( outFile );
		Pattern pat = Pattern.compile("([^\\:]*)[\\:](\\s*\\d*)[\\:](\\s*\\d*)[\\:](.*)");
		// initializations
		map = new HashMap< String, Map< String, List<String> > >();
		mapRules = new HashMap< String, String >();
		
		// process...
		readLines(InputFile);
		section1 = readChunk( a_str_Markup[3], a_str_Markup[4]);
		section3 = readChunk( a_str_Markup[7], a_str_Markup[8]);
		rules = readChunk( a_str_Markup[0], a_str_Markup[1]);
		fillRuleMap();
		fillMap();
		Iterator< String > iterator = section1.iterator();
		output.format(str_filehead);
		while ( iterator.hasNext() ) {             // loop on each field
			line = new String(iterator.next());
			Matcher m = pat.matcher(line);         // search begin
		    if (m.find(  )) {
		    	if( !matchedfile.equals(m.group(1)) ){
		    		if(!matchedfile.equals("")){
		    			output.format("        </file>\n");
		    		}
		    		output.format("        <file path=\"%s\">\n", m.group(1) );
		    		matchedfile = m.group(1);
		    	}
		    	rule = findRule(m.group(1), m.group(2).trim() + ":" + m.group(3).trim());
		    	output.format( str_violformat, m.group(2).trim(), m.group(4).trim(), rule );
		    }
		}
		if(!matchedfile.equals("")){
			output.format("        </file>\n");
		}
		output.format(str_filetail);
		if ( output != null )
			output.close();
	}
	
	private void readLines(File InputFile) throws FileNotFoundException {
		Scanner scanner = new Scanner(InputFile);
		lines = new LinkedList< String >();
		// loop on each field
		while (scanner.hasNextLine()) {
			String line = new String(scanner.nextLine());
			lines.add(line);
		}
		scanner.close();
	}
	
	private List< String > readChunk( String strStart, String strEnd  ){
		int start = lines.indexOf(strStart);
		int end = lines.indexOf(strEnd);
		return lines.subList(start, end);
	}
	
	private String findRule(String file, String ln){
		String rule = new String("");
		Map< String, List<String> > mapViols;
		if(map.containsKey(file)){
			mapViols = map.get(file);
			for( Map.Entry<String, List<String>> entry  : mapViols.entrySet()){
				List<String> lnNums = entry.getValue();
				if(lnNums.contains(ln)){
					rule = entry.getKey();
					lnNums.remove(ln);
					if(lnNums.isEmpty()){         // remove violation
						mapViols.remove(rule);
					}else{
						mapViols.put(rule, lnNums);
					}
					map.put(file, mapViols);
					break;
				}
			}
		}
		return rule;
	}
	
	private void fillRuleMap(){
		String line = new String();
		Pattern pat = Pattern.compile("(\\([^\\)]*\\))\\s(.*)");
		Iterator< String > iterator = rules.iterator();
		while ( iterator.hasNext() ) {         // loop on each field
			line = iterator.next();
			Matcher m = pat.matcher(line);     // search begin
		    if (m.find(  )) {
		    	mapRules.put(new String(m.group(2)), new String(m.group(1)));
		    }
		}
	}
	
	private void fillLnNumMap(ListIterator< String > iterator,	List<String>  lstLnNum){
		String line = new String();
		Pattern pat = Pattern.compile("(\\s*\\d*)[\\:](\\d*)");
		while ( iterator.hasNext() ) { 
			line = iterator.next();
			Matcher m = pat.matcher(line);  
			if (m.find()) {               // a line number is found
				lstLnNum.add(m.group(1).trim() + ":" + m.group(2));
			} 
			else{              // else leave
				break;
			}
		}
	}
	
	private void fillViolMap(ListIterator< String > iterator, Map< String, List<String> > mapViol){
		String line = new String();
		Pattern pat = Pattern.compile("(Matches\\sdetected\\sin\\sfile\\s)(.*)");
		while ( iterator.hasNext() ) { 
			line = iterator.next();
			Matcher m = pat.matcher(line);  
			if (m.find(  )) {                // new file, so leave
				iterator.previous();
				break;
			}
			if( mapRules.containsKey(line.trim())){  // rule found, so fill line numbers 
				List<String>  lstLnNum = new LinkedList<String>();
				fillLnNumMap( iterator, lstLnNum );
				if(!lstLnNum.isEmpty()){             // for a rule some violations are found
					mapViol.put(mapRules.get(line.trim()), lstLnNum);
				}
			}
		}
	}
	
	private void fillMap(){
		Map< String, List<String> > mapViol;
		String line = new String();
		Pattern pat = Pattern.compile("(Matches\\sdetected\\sin\\sfile\\s)(.*)");
		ListIterator< String > iterator = section3.listIterator();
		while ( iterator.hasNext() ) {           // loop on each field
			line = iterator.next();
			Matcher m = pat.matcher(line);       // search begin
		    if (m.find(  )) {
		    	mapViol = new LinkedHashMap< String, List<String> >();  //insertion-ordered map
		    	fillViolMap( iterator, mapViol);
		    	if(!mapViol.isEmpty()){
		    		map.put(new String(m.group(2)), mapViol);  // insert violations for a file
		    	}
		    }
		}
	}
	
	private boolean matchHead(int lineNumber, String line){
		boolean bMatch = false;
		if( (1 == lineNumber) && (!line.matches	(a_str_headpat[lineNumber-1])) ){
			bMatch = false;
		}
		else if( (2 == lineNumber) && (!line.matches (a_str_headpat[lineNumber-1])) ){
			bMatch = false;
		}else{ 
			bMatch = true;
		}
		return bMatch;
	}
	
	public Map<String, String> getRules(File inputFile){
		mapRules = new HashMap< String, String >();
		try {
			readLines(inputFile);
			rules = readChunk( a_str_Markup[0], a_str_Markup[1]);
			fillRuleMap();
		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return mapRules;
		
	}
}
