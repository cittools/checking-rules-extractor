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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Scanner;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

import com.thalesgroup.extractor.core.Rule;
import com.thalesgroup.extractor.core.SonarRulesFormatter;

public class CoreKlocwork92 {

	private HashMap<String, String> priorities;

	private static final String str_toolmarkup = "<!-- KLOCWORK 9.2 -->\n";

	public CoreKlocwork92() {
		priorities = new HashMap<String, String>();
		priorities.put("1", "BLOCKER");
		priorities.put("2", "BLOCKER");
		priorities.put("3", "CRITICAL");
		priorities.put("4", "CRITICAL");
		priorities.put("5", "MAJOR");
		priorities.put("6", "MAJOR");
		priorities.put("7", "MINOR");
		priorities.put("8", "MINOR");
		priorities.put("9", "INFO");
		priorities.put("10", "INFO");
	}

	/**
	 * Extract the rules from html and xml klocwork files and write them in an output file
	 * @param inputHTMLDirPath The directory containing HTML files
	 * @param inputXMLDirPath The directory containing XML files
	 * @param optionalConfigFilePath The configuration file of your klocwork project (optional)
	 * @param outFilePath The file where the rules are going to be written
	 */
	public void extract(String inputHTMLDirPath, String inputXMLDirPath, String optionalConfigFilePath, String outFilePath){
		SonarRulesFormatter sonar = new SonarRulesFormatter(str_toolmarkup);
		Map<String, Rule> map = new LinkedHashMap<String, Rule>();

		/**
		 * Treatment of the config file, if present.
		 */
		List<String> enabledRules = new ArrayList<String>();
		Map<String,String> newSeverities = new HashMap<String, String>();
		Element root = null;
		if(optionalConfigFilePath!=null && !optionalConfigFilePath.equals("") && optionalConfigFilePath.endsWith(".xml")){
			File configFile = new File(optionalConfigFilePath);
			if (configFile.exists()){
				try {
					root = getElementRootOfXMLFile(configFile);
					NodeList nodesList = root.getElementsByTagName("error");
					for (int i=0;i<nodesList.getLength();i++){
						Element error = (Element)nodesList.item(i);
						if ("true".equals(error.getAttribute("enabled"))){
							enabledRules.add(error.getAttribute("id"));
						}
						//Keeping the new severity
						if (!error.getAttribute("severity").isEmpty()){
							newSeverities.put(error.getAttribute("id"), error.getAttribute("severity"));
						}
					}
				} catch (IOException e) {
					e.printStackTrace();
				}
				if (enabledRules.isEmpty()){
					System.err.println("List of enabled rules is empty. Converting all rules...");
				}
			}else {
				System.err.println("Config file "+optionalConfigFilePath+" doesn't exist. Converting all rules...");
			}
		}
		else {
			System.err.println("No XML config file given. Converting all rules...");
		}

		for (Rule r : getRulesFromHTML(inputHTMLDirPath)){
			if (root==null || (root!=null && enabledRules.contains(r.id)) || (root!=null && enabledRules.isEmpty())){
				if (newSeverities.containsKey(r.id)){
					r.priority = priorities.get(newSeverities.get(r.id));
				}
				map.put(r.id, r);
			}
		}
		for (Rule r : getRulesFromXML(inputXMLDirPath)){
			if (root==null || (root!=null && enabledRules.contains(r.id)) || (root!=null && enabledRules.isEmpty())){
				if (newSeverities.containsKey(r.id)){
					r.priority = priorities.get(newSeverities.get(r.id));
				}
				map.put(r.id, r);
			}
		}
		File outFile = new File(outFilePath);
		sonar.format(map, outFile);
	}

	/**
	 * Read all HTML files present in a directory and return a Hashmap containing the checkers name and their description
	 * @param inputDirPath Path to the directory containing the HTML files, corresponding to Klocwork default checkers
	 * @return A list containing the rules present in the file 
	 */
	public List<Rule> getRulesFromHTML(String inputDirPath){
		File inputDir = new File(inputDirPath);
		List<Rule> rules = new ArrayList<Rule>();
		if (inputDir == null){
			throw new IllegalArgumentException("inputDir must not be null");
		}
		if (!inputDir.isDirectory()){
			throw new IllegalArgumentException("inputDir must be a directory");
		}
		for (File html : inputDir.listFiles()){
			if (html.getName().endsWith(".html") || html.getName().endsWith(".htm")){
				try {
					List <Rule> list= getRuleFromHTML(html);
					for (Rule r : list){
						rules.add(r);
					}
				} catch (FileNotFoundException e) {
				}
			}
		}

		return rules;
	}

	/**
	 * Read one HTML file and return the rules present in this file.
	 * Pre-requisites : The HTML file has to contain a table with at least four columns corresponding to "Checker code", "Description", "Default severity", and "Enabled by default ?"
	 * @param htmlFile The file to read to extract the rules
	 * @return A list containing the rules present in the file
	 * @throws FileNotFoundException
	 */
	private List<Rule> getRuleFromHTML(File htmlFile) throws FileNotFoundException{
		Scanner scanner = new Scanner(htmlFile);
		StringBuilder sb = new StringBuilder();
		List<Rule> rules = new ArrayList<Rule>();
		String trBegin = "<tr>";
		String tdBegin = "<td>";
		String tableBegin = "<table id=\"sortable_table_id_0\"";
		String tableEnd = "</table>";
		while (scanner.hasNextLine()){
			sb.append(scanner.nextLine());
		}
		String doc = sb.toString();
		//Getting the good table
		doc = doc.substring(doc.indexOf(tableBegin));
		//Deleting the headers of the table and the elements after the table
		doc = doc.substring(doc.indexOf(tdBegin)+tdBegin.length(),doc.indexOf(tableEnd));
		String[] lines = doc.split(trBegin);
		//Parsing all lines (<tr></tr>)
		for (String line : lines){
			String[] cells = line.split(tdBegin);
			int i=1;
			Rule r = new Rule();

			//Parsing all cells (<td></td>)
			for (String cell : cells){
				if (cell.trim().isEmpty()){
					continue;
				}
				if (i==1){ //Checker code
					//Deleting the tags (<a...></a><br>) surrounding the wanted value
					cell = cell.substring(cell.indexOf(">")+1,cell.indexOf("</a>"));
					r.id = cell.trim();
					i++;
				}
				else if (i==2){ //Description
					//Deleting the last </td>
					cell = cell.substring(0,cell.indexOf("</td>"));
					r.name = replaceHTMLcodes(cell.trim());
					i++;
				}
				else if (i==3){ //Default Severity
					//Deleting the last </td>
					cell = cell.substring(0,cell.indexOf("</td>"));
					r.priority = priorities.get(cell.trim());
					i++;
				}
				else {
					i=-1;
				}
			}
			r.configKey = r.id;
			r.category = "Reliability";
			r.description = r.name;
			rules.add(r);
		}
		return rules;
	}

	/**
	 * Replace the HTML code, such as '&amp;', by its real value.
	 * @param str The string where there are modifications to do
	 * @return A copy of the string without HTMLcodes
	 */
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

	/**
	 * Read all XML files present in a directory and return a Hashmap containing the checkers name and their description
	 * @param inputDirPath Path to the directory containing the XML files, corresponding to Klocwork default checkers
	 * @return A list containing the rules present in the file
	 */
	public List<Rule> getRulesFromXML(String inputDirPath){
		File inputDir = new File(inputDirPath);
		List<Rule> rules = new ArrayList<Rule>();
		if (inputDir == null){
			throw new IllegalArgumentException("inputDir must not be null");
		}
		if (!inputDir.isDirectory()){
			throw new IllegalArgumentException("inputDir must be a directory");
		}

		for (File xml : inputDir.listFiles()){
			if (xml.getName().endsWith(".xml")){
				try {
					List<Rule> list = getRuleFromXML(xml);
					for (Rule r : list){
						rules.add(r);
					}
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
		}
		return rules;
	}

	/**
	 * Read one XML file and return the rules present in this file.
	 * @param xmlFile The file to read to extract the rules
	 * @return A list containing the rules present in the file
	 * @throws IOException
	 */
	private List<Rule> getRuleFromXML(File xmlFile) throws IOException{
		List<Rule> rules = new ArrayList<Rule>();
		Element root = getElementRootOfXMLFile(xmlFile);
		if (root==null){
			return rules;
		}
		//There are two kinds of "error" tags in Klocwork plugin files, that's why we are first treating "checker" tags.
		NodeList checkers = root.getElementsByTagName("checker");
		for (int i = 0;i<checkers.getLength();i++){
			Node checker = checkers.item(i);
			NodeList errors = checker.getChildNodes();
			for (int j=0; j<errors.getLength();j++){
				Node error = errors.item(j);
				if ("error".equals(error.getNodeName())){
					Element e = (Element)error;
					Rule r = new Rule();
					r.id = e.getAttribute("id");
					r.name = e.getAttribute("title");
					r.priority = priorities.get(e.getAttribute("severity"));
					r.configKey = r.id;
					r.category = "Reliability";
					r.description = r.name;
					rules.add(r);
				}
			}
		} 
		return rules;
	}

	/**
	 * Return the root element of an XML file
	 * @param xmlFile The XML file to read
	 * @return The root Element of the XML file
	 * @throws IOException
	 */
	private Element getElementRootOfXMLFile(File xmlFile) throws IOException{

		try {
			DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
			DocumentBuilder builder = factory.newDocumentBuilder();
			Document document = builder.parse(xmlFile);
			return  document.getDocumentElement();
		} catch (ParserConfigurationException e) {
			e.printStackTrace();
		} catch (SAXException e) {
			e.printStackTrace();
		}
		return null;
	}

}
