// This code creates a Wi-Fi access point and a web server, allowing users to send, view, and manage messages via a simple web interface.
// Messages are stored in SPIFFS, ensuring persistence even after the device is reset or the page is refreshed.

// **TODO**
// 1. Add an option to delete all saved messages.
// 2. Ensure messages are displayed persistently on the web page, even after the page is refreshed or reloaded.
// 3. Allow retrieval of the latest message separately through a simple HTTP endpoint.

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <SPIFFS.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

const char *ssid = "EAHS_Router";
const char *password = "Furkan31";

WiFiServer server(80);
String messages = "";      // Stores all messages
String latestMessage = ""; // Stores the most recent message

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);
    Serial.println();
    Serial.println("Configuring access point...");

    if (!WiFi.softAP(ssid, password)) {
        log_e("Soft AP creation failed.");
        while (1);
    }

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.begin();

    Serial.println("Server started");

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("An error has occurred while mounting SPIFFS");
        return;
    }

    // Load messages from SPIFFS
    loadMessages();
}

void loop() {
    WiFiClient client = server.available();

    if (client) {
        Serial.println("New Client.");
        String request = ""; // To store the HTTP request
        bool isPostRequest = false;
        int contentLength = 0;

        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                Serial.write(c);
                request += c;

                // Look for end of request (blank line)
                if (request.endsWith("\r\n\r\n")) {
                    if (request.startsWith("POST")) {
                        isPostRequest = true;
                        contentLength = extractContentLength(request);
                        handlePostRequest(client, contentLength);
                    } else if (request.startsWith("GET")) {
                        handleGetRequest(client, request);
                    }
                    break;
                }
            }
        }
        client.stop();
        Serial.println("Client Disconnected.");
    }
}

void handleGetRequest(WiFiClient &client, const String &request) {
    // Check the URI requested
    if (request.indexOf("GET /latest") == 0) {
        // Serve the latest message as plain text
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/plain");
        client.println();
        client.print(latestMessage);
        client.println();
    } else {
        // Serve the main page
        sendHttpResponse(client, true);
    }
}

void handlePostRequest(WiFiClient &client, int contentLength) {
    char postData[contentLength + 1];
    int bytesRead = client.readBytes(postData, contentLength);
    postData[bytesRead] = '\0';

    String postString = String(postData);
    int messageIndex = postString.indexOf("message=") + 8;
    String messageContent = postString.substring(messageIndex);
    messageContent.replace('+', ' ');
    messageContent = urlDecode(messageContent);

    if (messageContent.length() > 0) {
        String clientIP = client.remoteIP().toString();
        latestMessage = messageContent; // Update the latest message
        String newMessage = "From IP " + clientIP + ": " + messageContent + "<br>";

        if (!messages.endsWith(newMessage)) {
            messages += newMessage;
            saveMessages();
        }
    }

    sendHttpResponse(client, false);
}

void sendHttpResponse(WiFiClient &client, bool isGetRequest) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println();

    client.println("<html><body>");
    client.println("<h2>Message Board</h2>");
    client.println("<form method='POST' action='/'>");
    client.println("Message: <input type='text' name='message'><br>");
    client.println("<input type='submit' value='Send Message'>");
    client.println("</form>");

    client.println("<h3>Messages:</h3>");
    client.println("<div style='border:1px solid black;padding:10px;'>");
    client.println(messages);
    client.println("</div>");
    client.println("</body></html>");

    client.println();
}

int extractContentLength(const String &request) {
    int index = request.indexOf("Content-Length:");
    if (index != -1) {
        int start = index + 15;
        int end = request.indexOf("\r\n", start);
        return request.substring(start, end).toInt();
    }
    return 0;
}

String urlDecode(String input) {
    String decoded = "";
    char temp[] = "00";
    unsigned int i, j;
    for (i = 0; i < input.length(); i++) {
        if (input[i] == '+') {
            decoded += ' ';
        } else if (input[i] == '%') {
            temp[0] = input[i + 1];
            temp[1] = input[i + 2];
            decoded += (char)strtol(temp, NULL, 16);
            i += 2;
        } else {
            decoded += input[i];
        }
    }
    return decoded;
}

void saveMessages() {
    File file = SPIFFS.open("/messages.txt", FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(messages);
    file.close();
}

void loadMessages() {
    File file = SPIFFS.open("/messages.txt", FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }
    while (file.available()) {
        messages += (char)file.read();
    }
    file.close();
}
