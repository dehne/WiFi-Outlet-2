/****
 * @file SimpleWebServer.h 
 * @version 0.2.0
 * 
 * This file is a portion of the package SimpleWebServer, a library that provides 
 * an ESP8266 Arduino sketch with the ability to service HTTP requests.
 * 
 * The typical basic pattern is to instantiate an instance of SimpleWebServer as a 
 * global in the sketch and then in the sketch's setup() function to invoke begin(), 
 * passing the WiFiServer the SimpleWebServer is to use. 
 * 
 * Next, still in setup(), attach the sketch-supplied methodHandler functions for 
 * the HTTP methods (GET, HEAD, POST, etc.) that the sketch will respond to. These are 
 * invoked whenever a repsonse to a client request arrives that specifies the 
 * corresponding method. A methodHandler is responsible for assembling the message and 
 * sending it to the client as the SimleWebServer's response to the request.
 * 
 * With the handlers attached, the SimpleWebServer is ready to go.
 * 
 * As the sketch runs, it should call the SimpleWebServer's run() member function 
 * often. That lets it do it's thing.
 * 
 * For example, suppose the sketch only wants to serve pages. In that case it need 
 * only attach two methodHandlers, one for the GET method and one for the HEAD method. 
 * (A conforming web server MUST implement the GET and HEAD methods, per RFC 9110.) 
 * When a page is requested by a client, the GET methodHandler is invoked. It is 
 * passed a reference to the SimpleWebServer that invoked it, a reference to the 
 * client that made the request, the path of the requested page (e.g., 
 * "/index.html") and the query string (basically the stuff at the end of a URI 
 * following the "?"). 
 * 
 * The handler's job is to create and send the complete message to the client -- both 
 * the headers and the content. To make the creating the headers easy, SimpleWebServer 
 * has ready-to-go headers for the commonly encountered situations that methodHandlers 
 * can easily incorporate into their messages. So, really, a methodHandler's work is 
 * mostly about creating the content. To ease that work, SimpleWebServer provides
 * methodHandler support member functions methodHandlers can use to get more 
 * information about the context of request and to do common tasks. (Admittedly there 
 * are few (i.e., exactly 1) at this point. I plan to write them as I encounter the 
 * need.)
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
//#define SWS_DEBUG                               // Uncomment to enable debug printing.
#define SWS_CLIENT_WAIT_MILLIS      (10000)     // Maximum millis() to wait for client.

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
const char swsBadRequestResponseHeaders[] = "400 Bad Request\r\n"
                                            "Connection: close\r\n\r\n";
/**
 * @brief   Typical response to a request for something the handler doesn't have. A methodHandler 
 *          can just make this be the entire message it sends.
 * 
 */
const char swsNotFoundResponseHeaders[] =   "404 Not Found\r\n"
                                            "Connection: close\r\n\r\n";

/**
 * @brief   Typical response when either the server does not recognize the request method, or 
 *          lacks the ability to fulfill the request.
 * 
 */
const char swsNotImplementedResponseHeaders[] = "501 Not Implemented\r\n"
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
         *          When called, a methodHandler is passed a pointer to the server it's the 
         *          handler for, a pointer to the HTTP client that made the request, and the path 
         *          and query string from the URI of the resource the client requested. 
         * 
         *          A methodHandler's job is to send the client the complete message that forms 
         *          the response to the client's request. A WiFiClient is, among other things, a 
         *          subclss of "Print". That means it's fine for a methodHandler to do things like 
         *          "client->printf();" as needed to get the job done.
         * 
         *          In doing its work, a methodHandler can use the SimpleWebServer's handler-
         *          support member functions to ask questons about the context of the request.
         *          For example, "server->httpMethod();" gets the HTTP method the client used in 
         *          the request. (Useful for methodHandlers that handle more than one method.)
         */
        using swsMethodHandler = void (*)(SimpleWebServer*, WiFiClient*, String, String);

        /**
         * @brief   Attach the sketch-supplied handler for the specified HTTP method.
         *          Calling this when there's an already-attached handler replaces the 
         *          existing handler with the new one. 
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
         * @brief   methodHandler support member function: Return the HTTP Method requested by the client. 
         *          Returns swsBAD_REQ if no request is being processed.
         * 
         * @return swsHttpMethod_t 
         */
        swsHttpMethod_t httpMethod();

        /**
         * @brief   Utility member function: Return the ix-th word in source where "words" are 
         *          ' '-separated strings of characters.
         * 
         * @param source    The string from which the words are extracted.
         * @param ix        The index of the word being requwested. The first word is word 0.
         * @return String   The ix-th word, "" if none exists.
         */
        static String getWord(String source, uint8_t ix = 0);

    private:
        /**
         * @brief Instance variables.
         * 
         */
        WiFiServer* server;                                 // The WiFiServer we talk to.
        swsMethodHandler handlers[SWS_METHOD_COUNT];        // The list of handlers for the various HTTP methods.
                                                            //  plus one for undefined requests.
        swsHttpMethod_t trMethod;                           // When servicing s request, the method asked for, else swsBAD_REQ.
        String trPath;                                      // When servicing a request, the path to the target resource, else "".
        String trQuery;                                     // When servicing a request, the query foe the target resource, else "".

        /**
         * @brief Utility method: Get and return the HTTP client's request.
         * 
         * @param client    The  HTTP client who has a request.
         * @return String   The HTTP client's request; empty string if none
         */
        String getClientRequest(WiFiClient client);
        
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