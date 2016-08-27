//
//  CaseListReporter.hh
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 8/26/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//

#include "catch.hpp"


using namespace Catch;


/** Custom reporter that logs a line for every test file and every test case being run.
    Use CLI args "-r list" to use it. */
struct CaseListReporter : public ConsoleReporter {
    CaseListReporter( ReporterConfig const& _config )
    :   ConsoleReporter( _config )
    {}

    virtual ~CaseListReporter() CATCH_OVERRIDE { }
    static std::string getDescription() {
        return "Logs a line for every test case";
    }

    virtual void testCaseStarting( TestCaseInfo const& _testInfo ) CATCH_OVERRIDE {
        auto file = _testInfo.lineInfo.file;
        if (file != _curFile) {
            _curFile = file;
            auto slash = file.rfind('/');
            stream << "## " << file.substr(slash+1) << ":\n";
        }
        stream << "\t>>> " << _testInfo.name << "\n";
        ConsoleReporter::testCaseStarting(_testInfo);
    }
    virtual void testCaseEnded( TestCaseStats const& _testCaseStats ) CATCH_OVERRIDE {
        ConsoleReporter::testCaseEnded(_testCaseStats);
    }

    std::string _curFile;
};

REGISTER_REPORTER( "list", CaseListReporter )
