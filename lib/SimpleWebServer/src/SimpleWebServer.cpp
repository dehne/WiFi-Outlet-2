/****
 * @file SimpleWebServer.cpp
 * @version 1.0.0
 * @date August 28, 2023
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

// Public member functions.

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

    // Get the next waiting client, if any.
    WiFiClient client = server->accept();

    // The existence of a client means there's a request to process.
    if (client) {
        #ifdef SWS_DEBUG
        Serial.print("Client connected.\n");
        #endif

        // Get the request block into clientMessageStartLine and clientMessageHeaders
        getClientMessage(&client);
        #ifdef SWS_DEBUG
        Serial.print("Got this request:\n");
        Serial.print(clientMessageStartLine);
        #endif

        // Analyze the request
        String reqMethod = getWord(clientMessageStartLine);         // The first word in the request is the HTTP method.

        trPath = getWord(clientMessageStartLine, 1);                // The second word in the request is the "origin form" URI.
        trQuery = "";                                               // From the URI, extract the query portion, if any.
        int queryStart = trPath.indexOf("?");
        if (queryStart != -1) {
            trQuery = trPath.substring(queryStart + 1);
            trPath = trPath.substring(0, queryStart);               // What remains is the path portion.
        }
        #ifdef SWS_DEBUG
        Serial.printf("The request method is %s. The resource path is \"%s\" and the query is \"%s\".\n", 
            reqMethod.c_str(), trPath.c_str(), trQuery.c_str());
        #endif

        // Find the methodHandler for the requested method and dispatch it to send the response message to the client.
        for (uint8_t i = 0; i < SWS_METHOD_COUNT; i++) {
            if (i == SWS_METHOD_COUNT - 1 || reqMethod.equals(swsMethodNames[i])) {
                trMethod = (swsHttpMethod_t)i;
                (*handlers[i])(this, &client, trPath, trQuery);
                break;
            }
        }
        // That's it. We're done. Clean things up for when the next request comes.
        client.stop();
        clientMessageHeaders = "";
        clientMessageStartLine = "";
        trPath = "";
        trQuery = "";
        trMethod = swsBAD_REQ;
        #ifdef SWS_DEBUG
        Serial.print("Client disconnected.\n");
        #endif
    }
}

/**
 * getHttpMethod()
 */
swsHttpMethod_t SimpleWebServer::httpMethod() {
    return trMethod;
}

/**
 * clientStartLine()
 */
String SimpleWebServer::clientStartLine() {
    return clientMessageStartLine;
}

/**
 * clientHeadrs() 
 */
String SimpleWebServer::clientHeaders() {
    return clientMessageHeaders;
}

/**
 * clientBody()
 */
String SimpleWebServer::clientBody() {
    return clientMessageBody;
}

/**
 * getHeader()
 */
String SimpleWebServer::getHeader(String headerName) {
    unsigned int cursor = 0;
    String answer = "";
    while (cursor < clientMessageHeaders.length()) {
        // cursor is the index of the first character of the current header line
        int lineEndIx = clientMessageHeaders.indexOf("\n", cursor + headerName.length() + 1);
        if (lineEndIx == -1) {
            #ifdef SWS_DEBUG
            Serial.printf("[getHeader] Header \"%s\" not found in\n",
                headerName.c_str());
            for (unsigned int i = 0; i < clientMessageHeaders.length(); i++) {
                char c = clientMessageHeaders.charAt(i);
                if (c <= ' ' || c == '%' || c > '~') {
                    Serial.printf("%%%02x", (uint8_t)c);
                } else {
                    Serial.print(c);
                }
            }
            Serial.print("\n");
            #endif
            break;
        }
        // lineEndIx is the index of the '\n' character at the end of the line
        if (headerName.equalsIgnoreCase(clientMessageHeaders.substring(cursor, cursor + headerName.length()))) {
            // answer is the value of the header named neaderName. (The +2 skips the ": " following the name)
            answer = clientMessageHeaders.substring(cursor + headerName.length() + 2, lineEndIx);
            break;
        }
        cursor = lineEndIx + 1;
    }
    return answer;
}

/**
 * getFormDatum()
 */
String SimpleWebServer::getFormDatum(String datumName) {
    // If the message body doesn't contain the right kind of data we won't find what's asked for
    if (!getHeader(SWS_CONTENT_TYPE_HDR).equals(SWS_FORM_CONTENT_HDR)) {
        Serial.print("[getFormDatum] Message body not the right type to contain form data.\n");;
        return "";
    }
    unsigned int cursor = 0;
    String item;
    String lookFor = datumName + "=";

    // Go through the clientMessagebody
    while (cursor < clientMessageBody.length()) {
        // Get the next form data item
        int ampIx = clientMessageBody.indexOf("&", cursor + 1);
        if (ampIx == -1) {
            item = clientMessageBody.substring(cursor);
            cursor = clientMessageBody.length();
        } else {
            item = clientMessageBody.substring(cursor, ampIx);
            cursor = ampIx + 1;
        }
        // Check this item to see if it's the one we're looking for
        if (lookFor.equals(item.substring(0, lookFor.length()))) {
            item = item.substring(lookFor.length());
            // un-URLencode item
            item.replace("+", " ");
            int pctIx = item.indexOf('%');
            while (pctIx != -1){
                if (item.charAt(pctIx + 1) == '%') {
                    // "%%" --> "%"
                    item = item.substring(0, pctIx) + item.substring(pctIx + 1);
                } else {
                    // "%xx" -> hexStringToChar("xx")
                    char converted = 0;
                    for (uint8_t i = 0; i < 2; i++) {
                        char c = item.charAt(pctIx + i + 1);
                        converted *= 16;
                        if (c >= '0' && c <= '9') {
                            converted += c - '0';
                        } else if (c >= 'A' && c <= 'F') {
                            converted += c - 'A' + 10;
                        } else if (c >= 'a' && c <= 'f') {
                            converted += c - 'a' + 10;
                        } else {
                            Serial.printf("[getFormDatum] Bad URL encoding char '%c' ignored\n", c);
                        }
                    }
                    item = item.substring(0, pctIx) + String(converted) + item.substring(pctIx + 3);
                }
                pctIx = item.indexOf('%');
            }
            #ifdef SWS_DEBUG
            Serial.printf("[getFormDatum] Found form datum \"%s\". Value is \"%s\".\n", 
                datumName.c_str(), item.c_str());
            #endif
            return item;
        }
    }
    #ifdef SWS_DEBUG
    Serial.printf("[getFormDatum] Couldn't find a form datum named \"%s\".\n", datumName.c_str());
    #endif
    return "";
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
 * getClientMessage()
 */
void SimpleWebServer::getClientMessage(WiFiClient* client) {
    unsigned long curMillis = millis();
    unsigned long lastMillis = curMillis;

    #ifdef SWS_DEBUG
    Serial.print("[getMessage] Getting start-line\n");
    #endif

    // Get the start-line from the message. It's the first line.
    clientMessageStartLine = "";
    while (client->connected() && curMillis - lastMillis < SWS_CLIENT_WAIT_MILLIS) {
        if (client->available()) {
            char c = client->read();
            if (c != '\r') {
                if (c != '\n') {
                    clientMessageStartLine += c;
                    lastMillis = curMillis;
                } else {
                    break;
                }
            }
        }
    }
    #ifdef SWS_DEBUG
    Serial.printf("[getMessage] Got start-line: \"%s\"\n", clientMessageStartLine.c_str());
    #endif
    
    // Get the HTTP Headers from the message. The end of these is marked by a blank line
    lastMillis = curMillis = millis();
    clientMessageHeaders = "";
    while(client->connected() && curMillis - lastMillis < SWS_CLIENT_WAIT_MILLIS) {
        curMillis = millis();
        if (client->available()) {
            char c = client->read();
            if (c != '\r') {    // Ignore <CR> chars
                if (c == '\n' && clientMessageHeaders.charAt(clientMessageHeaders.length() - 1) == '\n') {
                    break;
                }
                clientMessageHeaders += c;
                lastMillis = curMillis;
            }
        }
    }
    #ifdef SWS_DEBUG
    Serial.printf("[getMessage] Got headers: \"%s\"\n", clientMessageHeaders.c_str());
    #endif

    // Get the message body, if any. Gotten as-is, no '\r' removal.
    int bodyLength = getHeader(SWS_CONTENT_LENGTH_HDR).toInt();
    clientMessageBody = "";
    int charsRead = 0;
    while(client->connected() && charsRead < bodyLength && curMillis - lastMillis < SWS_CLIENT_WAIT_MILLIS) {
        curMillis = millis();
        if (client->available()) {
            clientMessageBody += (char)client->read();
            charsRead++;
            lastMillis = curMillis;
        }
    }
    #ifdef SWS_DEBUG
    if (bodyLength > 0) {
        Serial.printf("[getMessage] Got message body: \"%s\"\n", clientMessageBody.c_str());
    } else {
        Serial.print("[getMessage] No message body present.\n");
    }
    #endif
    if (curMillis - lastMillis >= SWS_CLIENT_WAIT_MILLIS) {
        Serial.print("[getMessage] Client timed out before we all data recieved.\n");
        Serial.printf("Start-line: \"%s\"\n", clientMessageStartLine.c_str());
        Serial.printf("Headers: \"%s\"\n", clientMessageHeaders.c_str());
        Serial.printf("Message body: \"%s\"\n", clientMessageBody.c_str());
        clientMessageStartLine = "";
        clientMessageHeaders = "";
        clientMessageBody = "";
    }
}

/**
 * defaultGetAndHeadHandler()
 */
void SimpleWebServer::defaultGetAndHeadHandler(SimpleWebServer* server, WiFiClient* httpClient, String path, String query) {
    httpClient->print(swsNotFoundResponse);
}

/**
 * defaultUnimplementedHandler()
 */
void SimpleWebServer::defaultUnimplementedHandler(SimpleWebServer* server, WiFiClient* httpClient, String path, String query) {
    httpClient->print(swsNotImplementedResponse);
}

/**
 * defaultBadHandler
 */
void SimpleWebServer::defaultBadHandler(SimpleWebServer* server, WiFiClient* httpClient, String path, String query) {
    httpClient->print(swsBadRequestResponse);
}