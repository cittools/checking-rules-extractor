package com.thalesgroup.extractor.cppcheck;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.HashMap;
import java.util.Map;
import java.util.Scanner;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.thalesgroup.extractor.core.Rule;
import com.thalesgroup.extractor.core.SonarRulesFormatter;

public class CoreCppcheck143 {

	private final static Map<String, String> severities = new HashMap<String, String>();
	private Map<String, Rule> rules;
	private  static final String str_toolmarkup = "<!-- CPPCHECK 1.43 -->\n";

	public CoreCppcheck143() {
		if (severities.isEmpty()){
			severities.put("error", "CRITICAL");
			severities.put("style", "MINOR");
			severities.put("possibleError", "MAJOR");
			severities.put("possibleStyle", "INFO");
		}
		rules = new HashMap<String, Rule>();
	}

	/**
	 * Check if there is a rule in the given line of code and add the rule in the map "rules" if it is the case.
	 * @param lineOfCode The line of the source 
	 */
	private void addRule(String lineOfCode){
		Pattern pattern = Pattern.compile("^\\s*(?:reportErr|reportError)\\(\\s*[\\w-]+?\\s*,\\s*(.+?)\\s*,\\s*\\\"(.+?)\\\"\\s*,\\s*.+?\\s*\\).*$");
		Matcher matcher = pattern.matcher(lineOfCode);
		if(matcher.matches()){
			Rule r = new Rule();
			String key = matcher.group(2);
			r.id = key;
			r.configKey = key;
			r.name = key;
			r.category = "Reliability";
			r.description = key;
			String rawSeverity = matcher.group(1);
			String[] splittedSeverity = rawSeverity.split("::");
			if (splittedSeverity.length >= 2){
				String severity = splittedSeverity[1];
				if (severity.contains(" ")){
					String[] cleanedSeverity = severity.split(" ");
					severity = cleanedSeverity[0];
				}
				r.priority = severities.get(severity.trim());
			}
			else {
				r.priority = "MAJOR";
			}
			rules.put(r.id, r);
		}
	}

	/**
	 * Open a file and read it to add rules in the map.
	 * @param input The file to read
	 * @throws FileNotFoundException
	 */
	private void readFile(File input) throws FileNotFoundException{
		Scanner scanner = new Scanner(input);
		while (scanner.hasNextLine()){
			String lineOfCode = scanner.nextLine();
			addRule(lineOfCode);
		}
		scanner.close();
	}

	/**
	 * Read all the files and directories present in the given directory and add the founded rules.
	 * @param directory The file or directory to read
	 */
	private void readDirectory(File directory){
		if (!directory.isDirectory()){
			return;
		}
		for (File file : directory.listFiles()){
			if (file.isDirectory()){
				readDirectory(file);
			}
			else{
				try {
					readFile(file);
				} catch (FileNotFoundException e) {
					e.printStackTrace();
				}
			}
		}
	}

	/**
	 * Extract the cppcheck rules and convert them into Sonar rules
	 * @param cppcheckLib The lib directory which can be found 
	 * @param outputFile
	 */
	public void extract(File cppcheckLib, File outputFile){
		readDirectory(cppcheckLib);
		SonarRulesFormatter sonar = new SonarRulesFormatter(str_toolmarkup);
		sonar.format(rules, outputFile);
	}

}
