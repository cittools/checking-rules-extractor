# Copyright (c) 2012 Thales Global Services SAS
# 
# Author : Robin Jarry
# 
# The MIT license. See LICENSE file for details

import sys
import os
import re
from sonar_rules_extractor.rule import Rule
from sonar_rules_extractor.extractor import RuleExtractor, ExtractorArgumentError

MSG_HEADER = re.compile(r'^:(?P<msg_id>\w+):\s*\*?(.*?)\*?$', re.MULTILINE)
NEWLINE_REX = re.compile(r'[\r\n]')
SPACE_REX = re.compile(r'\s+')

class PyLintExtractor(RuleExtractor):
    
    tool = 'pylint'
    
    def extract(self, *args):
        if len(args) == 0:
            # read from stdin
            print >>sys.stderr, 'Reading from standard input...' 
            input_stream = sys.stdin
        else:
            # read from file, only first argument is considered
            input_file = args[0]
            if not os.path.exists(input_file):
                raise ExtractorArgumentError('File "%s" not found.' % input_file)
            else:
                input_stream = open(input_file, 'r')
        
        input_buffer = input_stream.read()
        
        tokens = MSG_HEADER.split(input_buffer)
        
        rules = []
        while tokens:
            msg_id = tokens.pop(0).strip()
            if not msg_id:
                continue
            if len(tokens) > 2:
                title = tokens.pop(0).strip()
                description = tokens.pop(0).strip()
                description, _ = NEWLINE_REX.subn(' ', description)
                description, _ = SPACE_REX.subn(' ', description)
                
                if msg_id.startswith('F'): # Fatal
                    priority = 'BLOCKER'
                elif msg_id.startswith('E'): # Error
                    priority = 'CRITICAL'
                elif msg_id.startswith('W'): # Warning
                    priority = 'MAJOR'
                elif msg_id.startswith('R'): # Refactor
                    priority = 'MINOR'
                else: # For all other message types: Convention, etc.
                    priority = 'INFO'
                
                rules.append(Rule(key=msg_id, 
                                  name=title, 
                                  configKey=msg_id, 
                                  description=description, 
                                  priority=priority))
        return rules
