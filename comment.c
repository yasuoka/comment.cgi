/*
 * Require:
 *	- enable "Option ExecCGI"
 *	- add below lines to the .htaccess
 *	   AddHandler cgi-script .cgi
 *	   AddHandler server-parsed .html
 *
 * How to install:
 *	(to use TZ=Asia/Tokyo from jail)
 *	% sudo mkdir -p /var/www/usr/share/zoneinfo/Asia
 *	% sudo cp /usr/share/zoneinfo/Asia/Tokyo \
 *	    /var/www/usr/share/zoneinfo/Asia/
 *	% cc -Wall -static -o comment.cgi comment.c
 *	% touch comment-data.txt
 *	% sudo chown www comment-data.txt
 *
 * HTML example:
 *	<form action="./comment.cgi" id="commentForm" method="POST">
 *	  お名前: <input type="text" name="name" size="10">
 *	  コメント: <input type="text" name="comment" size="40">
 *	  <input type="submit">
 *	</form>
 *      <ul>
 *      <!--#include file="comment-data.txt" -->
 *      </ul>
 *
 * See also the comment on top of comment_subr.c.
 *
 * Author: YASUOKA Masahiko <yasuoka@yasuoka.net>
 * $Id: comment.c 138101 2012-06-25 08:03:51Z yasuoka $
 */
#define CHECK_CAPTCHA	1
#include "captcha.c"
#include "comment_subr.c"

int
main(int argc, char *argv[])
{
	extern char *tzname[2];

	setenv("TZ", "Asia/Tokyo", 1);	// time zone
	comment_cgi(
	    "comment-data.txt",		// file name of comment data.
	    "comment.html",		// file name of HTML displays the form
	    "MAILFROM@example.jp",	// sender mail address of hte notice
	    "MAILTO@example.jp");	// recipient mail address of the notice

	exit(EXIT_SUCCESS);
}
