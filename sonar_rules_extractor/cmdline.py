# Copyright (c) 2012 Thales Global Services SAS
# 
# Author : Robin Jarry
# 
# The MIT license. See LICENSE file for details

import sys
import inspect
import pkgutil
from optparse import OptionParser
from xml.dom.minidom import Document

import sonar_rules_extractor
from sonar_rules_extractor import tools as extractor_tools
from sonar_rules_extractor.extractor import RuleExtractor, ExtractorArgumentError

USAGE = '''%prog [options] tool <input args...>

  <input args...> depend on the tool. Generally, if no args are provided,
  standard input is read.'''

#------------------------------------------------------------------------------
def run():
    
    parser = OptionParser(usage=USAGE, 
                          version=sonar_rules_extractor.__version__)
    
    parser.add_option('-p', '--plugin', dest='plugins', metavar='MODULE', 
                      action='append', default=[],
                      help='Before extraction, import the specified module '
                           'and look for classes that inherit "Extractor" which '
                           'can be used in addtion to the built-in ones. '
                           'This option can be used multiple times.')
    parser.add_option('-f', '--format-xml', dest='format', action='store_true', default=False,
                      help='Pretty format XML output.')
    parser.add_option('-l', '--list-tools', dest='list_tools', action='store_true', default=False,
                      help='List all available tools.')

    options, args = parser.parse_args()

    options.available_extractors = {}
    
    def is_rule_extractor_subclass(obj):
        return (inspect.isclass(obj) 
                and not obj is RuleExtractor 
                and issubclass(obj, RuleExtractor))
    
    # we walk through the "sonar_rules_extractor.extractor_tools" package looking for 
    # subclasses of RuleExtractor. Each matching class is added to the 
    # options.available_extractors hash table with the hash key corresponding 
    # to the "ExtractorSubClass.tool" field.
    for loader, name, _ in pkgutil.walk_packages(extractor_tools.__path__):
        sub_module = loader.find_module(name).load_module(name)
        for _, ext_cls in inspect.getmembers(sub_module, is_rule_extractor_subclass):
            options.available_extractors[ext_cls.tool] = ext_cls
    
    # Then, we import the modules passed with -p options to the command line
    # and look for the same subclasses.
    for plugin in options.plugins:
        plugin_module = __import__(plugin)
        for _, ext_cls in inspect.getmembers(plugin_module, is_rule_extractor_subclass):
            options.available_extractors[ext_cls.tool] = ext_cls

    if options.list_tools:
        print ', '.join(options.available_extractors.keys())
        sys.exit(0)
    
    if len(args) < 1:
        parser.error('Missing tool name')
    else:
        options.tool = args.pop(0)
    
    try:
        # resolution of the actual extractor class based on the tool command line argument.
        options.extractor_class = options.available_extractors[options.tool]
    except KeyError:
        parser.error('Tool "%s" not found. Available tools are: %s' % 
                     (options.tool, ', '.join(options.available_extractors.keys())))
    
    # instanciation of the actual extractor
    extractor = options.extractor_class()
    try:
        # the extract() method should return a list of Rule objects
        rules = extractor.extract(*args) or []
    except ExtractorArgumentError, err:
        # extractors can raise OptionError if the arguments are inconsistent
        parser.error(err)
    
    # XML document construction
    doc = Document()
    doc.appendChild(doc.createComment('EXTRACTED "%s" RULES FOR SONAR' % options.tool))
    rules_elt = doc.createElement('rules')
    for rule in rules:
        rules_elt.appendChild(rule.to_xml_element())
    doc.appendChild(rules_elt)
    
    # write to standard output
    if options.format:
        doc.writexml(sys.stdout, addindent='  ', newl='\n', encoding='utf-8')
    else:  
        doc.writexml(sys.stdout, encoding='utf-8')
        sys.stdout.write('\n')

