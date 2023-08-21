/****
 * @file SimpleWebServer.cpp
 * @version 0.1.0
 * 
 * This file is a portion of the package SimpleWebServer, a library that provides 
 * an ESP8266 Arduino sketch with the ability to service HTTP requests. See 
 * SimpleWebServer.h for details.
 * 
 *****
 * 
 * Copyright (C) 2023 D.L. Ehnebuske
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
#include "SimpleWebServer.h"

// Public member functions

/**
 * Constructor
 */
SimpleWebServer::SimpleWebServer() {
    trPath = "";
    trQuery = "";
    trMethod = swsBAD_REQ;
    handlers[swsGET] = defaultGetAndHeadHandler;
    handlers[swsHEAD] = defaultGetAndHeadHandler;
    for (uint8_t i = swsPOST; i < SWS_METHOD_COUNT - 1; i++) {
        handlers[i] = defaultUnimplementedHandler;
    }
    handlers[swsBAD_REQ] = defaultBadHandler;
}

/**
 * begin()
 */
void SimpleWebServer::begin(WiFiServer &svr) {
    server = &svr;
}

/**
 * attachHandler()
 */
void SimpleWebServer::attachMethodHandler(swsHttpMethod_t method, swsMethodHandler handler) {
    handlers[method] = handler;
}

/**
 * run()
 */
void SimpleWebServer::run() {
    const char* swsMethodNames[SWS_METHOD_COUNT] = {"GET", "HEAD", "POST", "PUT", "DELETE",
                                                    "CONNECT", "OPTIONS", "TRACE", "PATCH"};

    // Get the next waiting client, if any 
    WiFiClient client = server->accept();

    // The existence of a client means there's a request to process
    if (client) {
        Serial.print("Client connected.\n");

        // Get the client's request
        String request = getClientRequest(client);
        Serial.print("Got this request:\n");
        Serial.print(request);

        // Analyze the request
        String reqMethod = getWord(request);                // The first word in the request is the HTTP method

        trPath = getWord(request, 1);                       // The second word in the request is the URI relative to the server root
        trQuery = "";                                       // Extract the query portion, if any
        int queryStart = trPath.indexOf("?");
        if (queryStart != -1) {
            trQuery = trPath.substring(queryStart + 1);
            trPath = trPath.substring(0, queryStart);       // What remains is the path portion
        }
        Serial.printf("The request method is %s. The resource path is \"%s\" and the query is \"%s\".\n", 
            reqMethod.c_str(), trPath.c_str(), trQuery.c_str());

        // Find the handler for the requested method and dispatch it to get the message we should send to the client
        String message = "";
        for (uint8_t i = 0; i < SWS_METHOD_COUNT; i++) {
            if (i == SWS_METHOD_COUNT - 1 || reqMethod.equals(swsMethodNames[i])) {
                trMethod = (swsHttpMethod_t)i;
                message = (*handlers[i])(this, trPath, trQuery);
                break;
            }
        }
        
        // Send the response to the client
        Serial.print("Sending response:\n");
        Serial.print(message);
        client.print(message);

        client.stop();
        trPath = "";
        trQuery = "";
        trMethod = swsBAD_REQ;
        Serial.print("Client disconnected.\n");
    }
}

/**
 * getHttpMethod()
 */
swsHttpMethod_t SimpleWebServer::httpMethod() {
    return trMethod;
}

/**
 * getWord()
 */
String SimpleWebServer::getWord(String source, uint8_t ix) {
    int16_t startAt = 0;
    int16_t len = source.length();
    String answer = "";
    for (int16_t i = 0; i < ix; i++){
        while (startAt < len && source.charAt(startAt) == ' ') {
            startAt++;
        }
        while (startAt < len && source.charAt(startAt) != ' ') {
            startAt++;
        }
    }
    while (startAt < len && source.charAt(startAt) == ' ') {
        startAt++;
    }
    while (startAt < len && source.charAt(startAt) != ' ') {
        answer += source.charAt(startAt++);
    }
    return answer;
}

// Private member functions

/**
 * getClientRequest()
 */
String SimpleWebServer::getClientRequest(WiFiClient client) {
    unsigned long curMillis = millis();
    unsigned long lastMillis = curMillis;
    String request = "";
    while(client.connected() && curMillis - lastMillis < SWS_CLIENT_WAIT_MILLIS) {
        curMillis = millis();
        if (client.available()) {
            char c = client.read();
            if (c != '\r') {    // Ignore <CR> chars
                if (c == '\n' && request.charAt(request.length() - 1) == '\n') {
                    return request;
                }
                request += c;
            }
        }
    }
    Serial.print("[getRequest] Timed out. Request is:\n");
    Serial.println(request);
    return "";
}

/**
 * defaultGetAndHeadHandler()
 */
String SimpleWebServer::defaultGetAndHeadHandler(SimpleWebServer* server, String path, String query) {
    return "404 Not Found\r\n"
           "Connection: close\r\n"
           "\r\n";
}

/**
 * defaultUnimplementedHandler()
 */
String SimpleWebServer::defaultUnimplementedHandler(SimpleWebServer* server, String path, String query) {
    return "501 Not Implemented\r\n"
           "Connection: close\r\n"
           "\r\n";
}

/**
 * defaultBadHandler
 * 400 Bad Request
 */
String SimpleWebServer::defaultBadHandler(SimpleWebServer* server, String path, String query) {
    return "400 Bad Request\r\n"
           "Connection: close\r\n"
           "\r\n";
}