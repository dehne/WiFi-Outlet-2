/****
 * @file SimpleWebServer.h 
 * @version 1.0.0
 * @date August 28, 2023
 * 
 * This file is a portion of the package SimpleWebServer, a library that provides an ESP8266 or 
 * ESP32 Arduino sketch with the ability to service HTTP requests.
 * 
 * The typical usage pattern is to instantiate an instance of SimpleWebServer as a global in the 
 * sketch and then, in the sketch's setup() function, to invoke begin(), passing the up-and-running 
 * WiFiServer the SimpleWebServer is to use. 
 * 
 * Next, still in setup(), attach the sketch-supplied methodHandler functions for the HTTP methods 
 * (GET, HEAD, POST, etc.) that the sketch will respond to. These are invoked whenever a repsonse 
 * to a client request arrives that specifies the corresponding method. A methodHandler is 
 * responsible for two things 1) assembling the message that is the SimleWebServer's response to 
 * the request and 2) the sending the assembled message to the client.
 * 
 * With the handlers attached, the SimpleWebServer is ready to go.
 * 
 * As the sketch runs, it should call the SimpleWebServer's run() member function often. Calling 
 * run() lets the SimpleWebServer do its thing.
 * 
 * For example, suppose the sketch only wants to serve pages. In that case it need only attach 
 * two methodHandlers, one for the GET method and one for the HEAD method. (A conforming web 
 * server MUST implement the GET and HEAD methods, per RFC 9110.) When a client requests a page, 
 * the SimpleWebServer invokes the attached GET methodHandlerpassing a reference to the 
 * SimpleWebServer that invoked it, a reference to a WiFiClient object (this represents the client 
 * that made the request) the path of the requested page (e.g., "/index.html") and the query string 
 * (basically the stuff at the end of a URI following the "?"). 
 * 
 * As mentioned, the messageHandler creates the complete message and then sends it to the client 
 * using the provided WiFiClient. To ease this task, SimpleWebServer provides a number of 
 * messageHandler support facilities. See the definition of the messageHandler type, below for 
 * more details.
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

#pragma once
#ifndef Arduino_h
#include <Arduino.h>
#endif
#ifndef ESP8266WiFi_h
#include <ESP8266WiFi.h>
#endif

/*
 * Miscellaneous constants
 */
//#define SWS_DEBUG                                       // Uncomment to enable debug printing.
#define SWS_CLIENT_WAIT_MILLIS      (10000)             // Maximum millis() to wait for client.
#define SWS_CONTENT_LENGTH_HDR      "Content-length"    // Name of the HTTP Content-length header
#define SWS_CONTENT_TYPE_HDR        "Content-Type"      // Name of the HTTP Content-type header
#define SWS_FORM_CONTENT_HDR        "application/x-www-form-urlencoded" // The kind of data a form POST contains

/**
 * @brief   Type definition enumerating the HTTP methods together with swsBAD_REQ for requests that come to us 
 *          requesting some other would-be method. SWS_METHOD_COUNT is just a count of the foregoing.
 * 
 */
enum swsHttpMethod_t {swsGET, swsHEAD, swsPOST, swsPUT, swsDELETE,
                                swsCONNECT, swsOPTIONS, swsTRACE, swsPATCH, swsBAD_REQ, SWS_METHOD_COUNT};

/**
 * @brief   Typical header block for an HTTP GET or POST serving up an HTML page. A methodHandler 
 *          can simply use this as the first part of the messages it sends.
 * 
 */
const char swsNormalResponseHeaders[] =     "HTTP/1.1 200 OK\r\n"
                                            "Content-type:text/html\r\n"
                                            "Connection: close\r\n\r\n";

/**
 * @brief   Typical response to a request that's just wrong. For a malformed query string, for 
 *          example, a methodHandler can send this as the message
 * 
 */
const char swsBadRequestResponseHeaders[] = "HTTP/1.1 400 Bad Request\r\n"
                                            "Connection: close\r\n\r\n";
/**
 * @brief   Typical response to a request for something the handler doesn't have. A methodHandler 
 *          can just make this be the entire message it sends.
 * 
 */
const char swsNotFoundResponseHeaders[] =   "HTTP/1.1 404 Not Found\r\n"
                                            "Connection: close\r\n\r\n";

/**
 * @brief   Typical response when either the server does not recognize the request method, or 
 *          lacks the ability to fulfill the request.
 * 
 */
const char swsNotImplementedResponseHeaders[] = "HTTP/1.1 501 Not Implemented\r\n"
                                                "Connection: close\r\n"
                                                "\r\n";

class SimpleWebServer {
    public:
        /**
         * @brief Construct a new SimpleWebServer object.
         * 
         */
        SimpleWebServer();

        /**
         * @brief   Start the web server up using the specified WiFiServer.
         *
         * @param svr   The WiFi server to use.
         */
        void begin(WiFiServer &svr);

        /**
         * @brief   This is the definition of the function type methodHandlers must have.
         * 
         * @details A methodHandler is a sketch-supplied function that a SimpleWebServer calls to 
         *          handle a request made by an HTTP client. For it to be called, a methodHandler 
         *          is "attached" to the SimpleWebServer (via the attachMethodHandler() member 
         *          function) for one or more HTTP methods. Attachment is usually done in setup().
         * 
         *          When called, a methodHandler is passed a pointer to the SimpleWebServer it's 
         *          the handler for, a pointer to the WiFiClient representing the client that made 
         *          the request, and the path and query string from the URI of the resource the 
         *          client requested. 
         * 
         *          A methodHandler's job is to send the client the complete message that forms 
         *          the response to the client's request. A WiFiClient is, among other things, a 
         *          subclass of "Print". That means it's fine for a methodHandler to do things like 
         *          "client->printf();" as needed to get the job done.
         * 
         *          In doing its work, a methodHandler can use the SimpleWebServer's ready-to-go 
         *          HTTP headers for common situations and "messageHandler support" member 
         *          functions the messageHandler can use to find out about the context of the 
         *          request and to do common taske. For example, "server->httpMethod();" gets the 
         *          HTTP method the client used in making the request. (Useful for methodHandlers 
         *          that handle more than one method.)
         */
        using swsMethodHandler = void (*)(SimpleWebServer*, WiFiClient*, String, String);

        /**
         * @brief   Attach the sketch-supplied handler for the specified HTTP method. Calling 
         *          this when there's an already-attached handler replaces the existing handler 
         *          with the new one. 
         * 
         * @param method    The method the specified handler services.
         * @param handler   The method handler for the specified method.
         */
        void attachMethodHandler(swsHttpMethod_t method, swsMethodHandler handler);

        /**
         * @brief   The typical run() method. let's the web server do its thing. Call often. 
         * 
         */
        void run();
        
        /**
         * @brief   methodHandler support member function: Returns the HTTP Method used by the 
         *          client in its request.
         * 
         * @details Returns swsBAD_REQ pseudo-method if called when no request is being processed. 
         * 
         * @return swsHttpMethod_t
         */
        swsHttpMethod_t httpMethod();

        /**
         * @brief   methodHandler support member function: Returns the start-line portion of the client's 
         *          request message.
         * 
         * @details Returns "" if called when no request is being processed. 
         * 
         * @return String 
         */
        String clientStartLine();

        /**
         * @brief   methodHandler support member function: Return the http headers portion of the client's 
         *          request message.
         * 
         * @details Returns "" if called when no request is being processed. 
         * 
         * @return String
         */
        String clientHeaders();

        /**
         * @brief   methodHandler support member function: Return the message body portion of the 
         *          client's request message.
         * 
         * @details Returns "" if called when no request is being processed. 
         * 
         * @return String
         */
        String clientBody();

        /**
         * @brief   methodHandler support member function: Return the value of the HTTP header in 
         *          the client's message having the specified name, or "" if the the request has 
         *          no header with the specified name.
         * 
         * @details Returns "" if called when no request is being processed. 
         * 
         * @param headerName    Name of the HTTP header whose value is desired
         * @return String
         */
        String getHeader(String headerName);

        /**
         * @brief   Get the value of the named from datum from the application/x-www-form-urlencoded
         *          message body in a POST request.
         * 
         * @details E.g., if the messageBody is 
         *              "daily=on&dg1=on&dg1n=08%3A00&dg1f=12%3A00&dg2n=13%3A00"
         *          doing getHeader("dg1n") would return "08:00".
         * 
         *          Returns "" if the message body isn't application/x-www-form-urlencoded data or 
         *          if the requested datum isn't one of the ones in the message body or if no 
         *          request is being processed. The result is "un-URL-encoded," so you won't see 
         *          things like "%20" in what's returned.
         * 
         * @param datumName     The name of the requested datum.
         * @return String
         */
        String getFormDatum(String datumName);

        /**
         * @brief   methodHandler support member function: : Return the ix-th word in source 
         *          where "words" are ' '-separated strings of characters. Returns "" if no 
         *          ix-th word exists in source.
         * 
         * @param source    The string from which the words are extracted.
         * @param ix        The index of the word being requwested. The first word is word 0.
         * @return String
         */
        static String getWord(String source, uint8_t ix = 0);

    private:
        /**
         * @brief Instance variables.
         * 
         */
        WiFiServer* server;                                 // The WiFiServer we talk to.
        swsMethodHandler handlers[SWS_METHOD_COUNT];        // The list of handlers for the various HTTP methods.
                                                            //  plus one for requests specifying an undefined method.
        swsHttpMethod_t trMethod;                           // When servicing a request, the method asked for, else swsBAD_REQ.
        String clientMessageStartLine;                      // When servicing a request, the HTTP start line, else "".
        String clientMessageHeaders;                        // When servicing a request, the header block, else "".
        String clientMessageBody;                           // When servicing a request, the body text, else "".
        String trPath;                                      // When servicing a request, the path to the target resource, else "".
        String trQuery;                                     // When servicing a request, the query foe the target resource, else "".

        /**
         * @brief   Utility member function: Get the HTTP client's entire message,filling in 
         *          clientMessageStartLine, clientMessageHeaders and clientMessageBody
         * 
         * @details getClientMessage fetches everything the client has to say until it reads the 
         *          whole message or times out. In processing the input, it discards <CR> 
         *          characters, leaving the <LF> ('\n') characters as line endings. It then breaks 
         *          the result into the instance variable Strings clientMessageStartLine, 
         *          clientMessageHeaders and clientMessageBody. If reading from the client times 
         *          out, they are all set to "".
         * 
         * @param client    The  HTTP client who has a request.
         */
        void getClientMessage(WiFiClient* client);
        
        /**
         * @brief   The default HTTP GET and HEAD handler. It just sends the HTTP client 
         *          "404 Not Found" no matter what's asked for.
         * 
         * @param server        The server the handler is working for.
         * @param heetClient    The HTTP client we are to send the message to.
         * @param path          The path portion of the requested resource URI.
         * @param query         The query portion of the same; "" if none.
         */
        static void defaultGetAndHeadHandler(SimpleWebServer* server, WiFiClient* httpClient, String path, String query);

        /**
         * @brief   The default HTTP handler for unimplemented methods. It just sends the HTTP client 
         *          "501 Not Implemented" no matter what's asked for.
         * 
         * @param server        The server the handler is working for.
         * @param httpClient    The HTTP client we are to sent the message to.
         * @param path          The path portion of the requested resource URI.
         * @param query         The query portion of the same; "" if none.
         */
        static void defaultUnimplementedHandler(SimpleWebServer* server, WiFiClient* httpClient, String path, String query);

        /**
         * @brief   The default HTTP handler for a request that's not one of the defined 
         *          HTTP methods. It just sends "400 Bad Request" to the HTTP client.
         * 
         * @param server    The server the handler is working for.
         * @param path      The path portion of the requested resource URI.
         * @param query     The query portion of the same; "" if none.
         */
        static void defaultBadHandler(SimpleWebServer* server, WiFiClient* httpClient, String path, String query);
};