Comment CGI
===========

"Comment CGI" is a CGI program which accepts one line comment from
visitors of the web page.  The program will notice you by email when it
accepts a new comment.  The program is simply written by C language
and it doesn't depend almost anything.  Therefore it's easy to run in
a privillege separated process.  This is used at
http://yasuoka.net/~yasuoka/calendar.html


Prerequisite
------------

MTA running on 127.0.0.1:25/tcp

How to compile
--------------

    % cc -Wall -static -o comment.cgi comment.c
    % touch comment-data.txt
    % sudo chown www comment-data.txt

HTML example
------------

    <form action="./comment.cgi" id="commentForm" method="POST">
      Your name: <input type="text" name="name" size="10">
      Comment: <input type="text" name="comment" size="40">
      <input type="submit">
    </form>
    <ul>
    <!--#include file="comment-data.txt" -->
    </ul>
