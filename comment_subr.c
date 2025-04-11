/*
 * Comment CGI
 *
 * Require:
 * 	- MTA (mail transfer agent) must be running on 127.0.0.1:25/tcp
 *
 * Author: YASUOKA Masahiko <yasuoka@yasuoka.net>
 * $Id: comment_subr.c 138958 2013-09-09 00:54:18Z yasuoka $
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <stdbool.h>
#include <err.h>
#include <unistd.h>

#ifndef nitems
#define	nitems(_x)	(sizeof(_x) / sizeof((_x)[0]))
#endif

#define CGI_BUFSIZ	8192

#define	ASSERT(cond)							\
	do {								\
		if (!(cond))						\
			errx(EX_SOFTWARE, "ASSERT(%s) failed",	#cond);	\
	} while (0/* CONSTCOND */)

static void         comment_cgi (const char *, const char *, const char *, const char *);
static void         mail_notify (const char *, const char *, const char *, const char *, const char *, struct tm *);
static void         reply (const char *);
static const char  *escape (const char *, char *, int);
static void         url_decode (char *);

static void
comment_cgi(const char *file_name, const char *html_path, const char *mail_from,
    const char *mail_to)
{
	int          f;
	FILE        *fp;
	const char  *name, *comment, *code;
	char        *ap, *ap0, qs[CGI_BUFSIZ], tmbuf[80], buf[CGI_BUFSIZ];
	time_t       currtime;
	struct tm    currtm;

	time(&currtime);
	localtime_r(&currtime, &currtm);

	/*
	 * parse the request
	 */
	name = comment = code = "";
	if (fgets(qs, sizeof(qs), stdin) == NULL)
		err(EX_OSERR, "%s(): fgets", __func__);
	if (!feof(stdin))
		errx(-1, "%s(): input is too long", __func__);

	for (ap0 = qs; (ap = strsep(&ap0, "&\n")) != NULL;) {
		char *n, *v;

		if (*ap == '\0')
			continue;
		v = strchr(ap, '=');
		ASSERT(v != NULL);
		n = ap;
		*(v++) = '\0';
		url_decode(n);
		url_decode(v);
		if (strcmp(n, "name") == 0)
			name = v;
		else if (strcmp(n, "comment") == 0)
			comment = v;
		else if (strcmp(n, "captcha_code") == 0)
			code = v;
	}
#ifdef CHECK_CAPTCHA
	if (!captcha_code_is_valid(code)) {
		fprintf(stdout,
		    "Status: 403\r\nCache-Control: no-cache\r\n"
		    "Content-Type: text/html\r\n\r\n"
		    "<html><head><title>Access Forbitten</title></head>"
		    "<body><h1>Access Forbitten</h1></body></html>");
		return;
	}
#endif

	/*
	 * write the file.
	 */
	if ((f = open(file_name, O_RDWR | O_APPEND | O_EXLOCK)) < 0)
		err(EX_OSERR, "open(%s)", file_name);
	if ((fp = fdopen(f, "w+")) == NULL) {
		close(f);
		err(EX_OSERR, "fdopen(%s)", file_name);
	}
	strftime(tmbuf, sizeof(tmbuf), "%Y%m%d%H%M%S", &currtm);
	fprintf(fp, "<li id=\"c%s\">", tmbuf);
	fputs(escape(comment, buf, sizeof(buf)), fp);
	fputs(" - ", fp);
	fputs(escape(name, buf, sizeof(buf)), fp);
	fputs(" (", fp);
	strftime(tmbuf, sizeof(tmbuf), "%Y年%m月%d日 %H時%M分%S秒", &currtm);
	fputs(tmbuf, fp);
	fputs(")</li>\n", fp);
	fclose(fp);

	/*
	 * notice by email
	 */
	mail_notify(mail_from, mail_to, name, comment, code, &currtm);

	/*
	 * replay to the client.
	 */
	reply(html_path);
}

static void
mail_notify(const char *mail_from, const char *mail_to, const char *name,
    const char *comment, const char *code, struct tm *currtm)
{
	int                 s, is6;
	struct sockaddr_in  sin4;
	char                tmbuf[128];
	FILE               *mailfp;
	extern char        *__progname;

	memset(&sin4, 0, sizeof(sin4));
	sin4.sin_family = AF_INET;
	sin4.sin_len = sizeof(sin4);
	sin4.sin_addr.s_addr = htonl(0x7f000001);
	sin4.sin_port = htons(25);
#define	expect(_fp, _exp)						\
	do {								\
		char _buf[BUFSIZ];					\
		for (;;) {						\
			if (fgets(_buf, sizeof(_buf), (_fp)) == NULL)	\
				err(EX_OSERR, "fgets");			\
			if (isdigit((unsigned char)_buf[0]) &&	\
			    isdigit((unsigned char)_buf[1]) &&	\
			    isdigit((unsigned char)_buf[2]) &&	\
			    _buf[3] == '-')				\
				continue;				\
			if (strncmp(_buf, (_exp), strlen(_exp)) == 0)	\
				break;					\
			errx(EX_OSERR, "error on mail server: %s", _buf);\
		}							\
	} while (0/* CONSTCOND */)

	is6 = (strchr(getenv("REMOTE_ADDR"), ':') == 0)? 0 : 1;

	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		err(EX_OSERR, "socket");
	if (connect(s, (struct sockaddr *)&sin4, sizeof(sin4)) != 0)
		err(EX_OSERR, "connect");
	if ((mailfp = fdopen(s, "r+")) == NULL)
		err(EX_OSERR, "fdopen");

	expect(mailfp, "2");
	fprintf(mailfp, "EHLO localhost\r\n");
	expect(mailfp, "2");
	fprintf(mailfp, "MAIL FROM: <%s>\r\n", mail_from);
	expect(mailfp, "2");
	fprintf(mailfp, "RCPT TO: <%s>\r\n",   mail_to);
	expect(mailfp, "2");
	fprintf(mailfp, "DATA\r\n");
	expect(mailfp, "3");

	strftime(tmbuf, sizeof(tmbuf), "%F(%a) %H:%M:%S", currtm);
	fprintf(mailfp,
	    "To: %s\n"
	    "From: %s\n"
	    "Subject: [%s] new comment\n"
	    "Content-Type: text/plain; charset=UTF-8\n"
	    "Content-Transfer-Encoding: 8bit\n"
	    "MIME-Version: 1.0\n"
	    "\n"
	    "Date:       %s\n"
	    "Source:     %s%s%s:%s\n"
	    "Name:       %s\n"
	    "Comment:    %s\n"
	    "Code:       %s\n",
	    mail_to, mail_from, __progname, tmbuf,
	    (is6)? "[" : "", getenv("REMOTE_ADDR"), (is6)? "]" : "",
	    getenv("REMOTE_PORT"), name, comment, code);

	fprintf(mailfp, "\r\n.\r\n");
	expect(mailfp, "2");
	fclose(mailfp);

	close(s);
}

static void
reply(const char *html_path)
{
	char url[BUFSIZ], buf[BUFSIZ], *slash;

	ASSERT(getenv("HTTP_HOST") != NULL);
	ASSERT(getenv("REQUEST_URI") != NULL);

	strlcat(url, "http://", sizeof(url));
	strlcat(url, getenv("HTTP_HOST"), sizeof(url));
	strlcat(url, getenv("REQUEST_URI"), sizeof(url));
	slash = strrchr(url, '/');
	ASSERT(slash != NULL);
	*(slash + 1) = '\0';
	strlcat(url, html_path, sizeof(url));
	
	snprintf(buf, sizeof(buf),
	    "<html>\n"
	    "<head>\n"
	    "<meta http-equiv=\"refresh\" content=\"0; url=%s\">\n"
	    "</head>\n"
	    "<body>\n"
	    "<h1>Thank you for your comment.</h1>\n"
	    "<p>\n"
	    "This page will move to <a href=\"%s\">%s</a> automatically.\n"
	    "</p>\n"
	    "</body>\n"
	    "</html>\n",
	    url, url, url);

	fprintf(stdout,
	    "Status: 200\r\n"
	    "Cache-Control: no-cache\r\n"
	    "Content-Type: text/html\r\n"
	    "Content-Length: %llu\r\n"
	    "\r\n"
	    "%s",
	    (long long unsigned)strlen(buf), buf);
}

static const char *
escape(const char *str, char *buf, int bufsiz)
{
	int 	i, j, ii, n;
	struct {
		int         ch;
		const char *escaped;
		int         length;
	} xml_esc[] = {
#define	ENTRY(_ch, _str)	{ _ch, _str, sizeof(_str) - 1 }
		ENTRY('\t', " "),
		ENTRY('\n', " "),
		ENTRY('\r', " "),
		ENTRY('<',  "&lt;"),
		ENTRY('>',  "&gt;"),
		ENTRY('&',  "&amp;"),
		ENTRY('"',  "&quot;"),
		ENTRY('\'', "&apos;")
#undef ENTRY
	};

	n = strlen(str) + 1;
	buf[0] = '\0';
	for (i = 0, j = 0; i < n && j + 1 < bufsiz; i++) {
		for (ii = 0; ii < nitems(xml_esc); ii++) {
			if (str[i] != xml_esc[ii].ch)
				continue;
			if (j + xml_esc[ii].length >= bufsiz)
				goto end;	/* buffer is not enough */
			memcpy(buf + j, xml_esc[ii].escaped,
			    xml_esc[ii].length + 1);
			j += xml_esc[ii].length;
			break;
		}
		if (ii >= nitems(xml_esc)) {
			/* no need to escape */
			if (j + 1 >= bufsiz)
				goto end;	/* buffer is not enough */
			buf[j++] = str[i];
		}
	}
end:
	buf[j] = '\0';

	return buf;
}

static void
url_decode(char *str)
{
	int i, j, n;

#define XDIGIT(_c) (						\
	    ('0' <= (_c) && (_c) <= '9')? (_c) - '0' :		\
	    ('a' <= (_c) && (_c) <= 'f')? (_c) - 'a' + 10 :	\
	    ('A' <= (_c) && (_c) <= 'F')? (_c) - 'A' + 10 : -1)
	n = strlen(str) + 1;
	for (i = 0, j = 0; i < n; i++, j++) {
		if (str[i] == '+')
			str[j] = ' ';
		else if (str[i] == '%' && i + 3 < n &&
		    isxdigit(str[i + 1]) && isxdigit(str[i + 2])) {
			str[j] = XDIGIT(str[i + 1]) << 4 | XDIGIT(str[i + 2]);
			i++; i++;
		} else if (i != j)
			str[j] = str[i];
	}
	if (i != j)
		str[j] = '\0';
}
