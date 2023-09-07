/****
 * @file WebCmd.cpp
 * @version 1.0.0
 * @date September 6, 2023
 * 
 * This file is a portion of the package WebCmd, a library that provides an Arduino sketch 
 * with the ability to provide a simple command line UI on a web page.
 *  
 * See WebCmd.h for details.
 * 
 *****
 * 
 * CommandLine V0.2, September 2023
 * Copyright (C) 2020 D.L. Ehnebuske
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 * 
 ****/

#include "WebCmd.h"

/**
 * Constructor 
 */
WebCmd::WebCmd(CommandLine* clo) {
    commandLineObject = clo;
}

/**
 * doCommand()
 */
String WebCmd::doCommand(String inputLine) {
    commandLine = inputLine;
    commandLine.trim();
    String cmd = getWord();
    // Ignore zero-length commands
    if(cmd.length() == 0) {
        return "";
    }
    // Dispatch whatever commandHandler commandLineObject->getHandlerFor says is correct for the 
    // one named by cmd, and use its result as our answer.
    String answer = commandLineObject->getHandlerFor(cmd)(this);
    commandLine = "";
    return answer;
}

/**
 * getWord()
 */
String WebCmd::getWord(uint8_t ix) {
    int16_t startAt = 0;
    int16_t len = commandLine.length();
    String answer = "";
    for (int16_t i = 0; i < ix; i++){
        while (startAt < len && commandLine.charAt(startAt) == ' ') {
            startAt++;
        }
        while (startAt < len && commandLine.charAt(startAt) != ' ') {
            startAt++;
        }
    }
    while (startAt < len && commandLine.charAt(startAt) == ' ') {
        startAt++;
    }
    while (startAt < len && commandLine.charAt(startAt) != ' ') {
        answer += commandLine.charAt(startAt++);
    }
    return answer;
}

/**
 * getCommandLine()
 */
String WebCmd::getCommandLine() {
    return commandLine;
}
