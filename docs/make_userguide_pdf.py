#!/usr/bin/env python3
#++
#	FACILITY: A yet another gateway for MODBUS (TCP to RTU)
#
#	DESCRIPTION: Generator of the User Guide PDFs (English + Russian) in the classic
#		DEC/VSI documentation style: the title page with the document number block,
#		the legal page, a dotted-leaders table of contents, the numbered Preface,
#		chapters/sections, ruled tables, monospaced examples and vector diagrams.
#		The front matter is numbered by the roman numerals, the body - by the arabic
#		ones, exactly as the VSI manuals do.
#
#	USAGE:
#		$ python3 make_userguide_pdf.py <EN|RU> <output.pdf>
#
#	AUTHOR: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
#
#	CREATION DATE: 25-AUG-2026
#--
import sys

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer,
	PageBreak, Table, TableStyle, Preformatted, NextPageTemplate, KeepTogether)
from reportlab.platypus.tableofcontents import TableOfContents
from reportlab.graphics.shapes import Drawing, Rect, Line, String, Polygon, Group

# ----------------------------------------------------------------------------------------------
#	Fonts: the DejaVu family covers both the Latin and the Cyrillic scripts
# ----------------------------------------------------------------------------------------------
FDIR = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Serif',   FDIR + 'DejaVuSerif.ttf'))
pdfmetrics.registerFont(TTFont('SerifB',  FDIR + 'DejaVuSerif-Bold.ttf'))
pdfmetrics.registerFont(TTFont('SerifI',  FDIR + 'DejaVuSerif-Italic.ttf'))
pdfmetrics.registerFont(TTFont('Sans',    FDIR + 'DejaVuSans.ttf'))
pdfmetrics.registerFont(TTFont('SansB',   FDIR + 'DejaVuSans-Bold.ttf'))
pdfmetrics.registerFont(TTFont('Mono',    FDIR + 'DejaVuSansMono.ttf'))
pdfmetrics.registerFont(TTFont('MonoB',   FDIR + 'DejaVuSansMono-Bold.ttf'))

PAGE_W, PAGE_H = A4
LM, RM, TM, BM = 25*mm, 22*mm, 22*mm, 22*mm

VERSION   = 'X.00-07'
PUBDATE_EN = 'August 2026'
PUBDATE_RU = 'Август 2026'
ORG       = 'NaPi World & StarLet Squad collaboration'

# ----------------------------------------------------------------------------------------------
#	Styles (the VSI look: serif body, bold sans-ish headings, mono examples)
# ----------------------------------------------------------------------------------------------
S = {}
S['body']   = ParagraphStyle('body',   fontName='Serif',  fontSize=10, leading=13.5,
	spaceAfter=6, alignment=TA_LEFT)
S['bodyi']  = ParagraphStyle('bodyi',  parent=S['body'], fontName='SerifI')
S['chap']   = ParagraphStyle('chap',   fontName='SerifB', fontSize=20, leading=24,
	spaceBefore=0, spaceAfter=18)
S['sec']    = ParagraphStyle('sec',    fontName='SerifB', fontSize=13.5, leading=17,
	spaceBefore=14, spaceAfter=7)
S['subsec'] = ParagraphStyle('subsec', fontName='SerifB', fontSize=11.5, leading=15,
	spaceBefore=10, spaceAfter=5)
S['code']   = ParagraphStyle('code',   fontName='Mono',   fontSize=8.4, leading=10.6,
	leftIndent=6*mm, spaceBefore=4, spaceAfter=8)
S['tcell']  = ParagraphStyle('tcell',  fontName='Serif',  fontSize=9,  leading=11.6)
S['tcellm'] = ParagraphStyle('tcellm', fontName='Mono',   fontSize=8.4, leading=10.8)
S['thead']  = ParagraphStyle('thead',  fontName='SerifB', fontSize=9,  leading=11.6)
S['caption']= ParagraphStyle('caption',fontName='SerifB', fontSize=9.5, leading=12,
	spaceBefore=4, spaceAfter=10, alignment=TA_CENTER)
S['title1'] = ParagraphStyle('title1', fontName='SerifB', fontSize=24, leading=30)
S['title2'] = ParagraphStyle('title2', fontName='Serif',  fontSize=17, leading=22)
S['tmeta']  = ParagraphStyle('tmeta',  fontName='Serif',  fontSize=10.5, leading=16)
S['legal']  = ParagraphStyle('legal',  fontName='Serif',  fontSize=8.5, leading=11,
	spaceAfter=6)
S['toc0']   = ParagraphStyle('toc0', fontName='SerifB', fontSize=10.5, leading=15, leftIndent=0)
S['toc1']   = ParagraphStyle('toc1', fontName='Serif',  fontSize=9.5, leading=13, leftIndent=8*mm)

TBL_STYLE = TableStyle([
	('FONT',          (0,0), (-1,-1), 'Serif', 9),
	('LINEABOVE',     (0,0), (-1,0),  1.0, colors.black),
	('LINEBELOW',     (0,0), (-1,0),  0.6, colors.black),
	('LINEBELOW',     (0,-1),(-1,-1), 1.0, colors.black),
	('VALIGN',        (0,0), (-1,-1), 'TOP'),
	('TOPPADDING',    (0,0), (-1,-1), 3),
	('BOTTOMPADDING', (0,0), (-1,-1), 3),
	('LEFTPADDING',   (0,0), (-1,-1), 4),
	('RIGHTPADDING',  (0,0), (-1,-1), 4),
])

def tbl (a_head, a_rows, a_widths, a_mono_cols=()):
	l_data = [[Paragraph(h, S['thead']) for h in a_head]]
	for l_r in a_rows:
		l_data.append([Paragraph(c, S['tcellm'] if i in a_mono_cols else S['tcell'])
			for i, c in enumerate(l_r)])
	l_t = Table(l_data, colWidths=a_widths, repeatRows=1)
	l_t.setStyle(TBL_STYLE)
	return l_t

# ----------------------------------------------------------------------------------------------
#	Vector diagrams
# ----------------------------------------------------------------------------------------------
def s_box (a_g, a_x, a_y, a_w, a_h, a_lines, a_fs=8.5, a_fill=colors.whitesmoke, a_font='Sans'):
	a_g.add(Rect(a_x, a_y, a_w, a_h, fillColor=a_fill, strokeColor=colors.black, strokeWidth=1))
	l_n = len(a_lines)
	for i, l_t in enumerate(a_lines):
		l_ty = a_y + a_h/2 + (l_n - 1 - 2*i) * (a_fs + 2)/2 - a_fs*0.35
		a_g.add(String(a_x + a_w/2, l_ty, l_t, fontName=a_font, fontSize=a_fs,
			textAnchor='middle'))

def s_arrow (a_g, a_x1, a_y1, a_x2, a_y2, a_both=False):
	a_g.add(Line(a_x1, a_y1, a_x2, a_y2, strokeWidth=1.2))
	import math
	l_ang = math.atan2(a_y2 - a_y1, a_x2 - a_x1)
	for (l_tx, l_ty, l_a) in ([(a_x2, a_y2, l_ang)] + ([(a_x1, a_y1, l_ang + math.pi)] if a_both else [])):
		l_p1 = (l_tx - 7*math.cos(l_a - 0.42), l_ty - 7*math.sin(l_a - 0.42))
		l_p2 = (l_tx - 7*math.cos(l_a + 0.42), l_ty - 7*math.sin(l_a + 0.42))
		a_g.add(Polygon([l_tx, l_ty, l_p1[0], l_p1[1], l_p2[0], l_p2[1]],
			fillColor=colors.black, strokeColor=colors.black))

def dia_architecture (a_L):
	"""The data path: TCP clients -> gateway -> RS-485 bus with the devices."""
	l_d = Drawing(460, 170)
	g = Group(); l_d.add(g)
	# TCP clients (stacked)
	for i, l_dy in enumerate((110, 75, 40)):
		s_box(g, 6, l_dy, 92, 26, [a_L['dia_client'] + ' ' + str(i + 1)], 8)
	g.add(String(52, 20, a_L['dia_tcpside'], fontName='SerifI', fontSize=8, textAnchor='middle'))
	# Gateway
	s_box(g, 168, 52, 130, 68, ['mbusgw-t2r', a_L['dia_queue']], 9, colors.Color(0.88, 0.92, 1))
	# arrows clients -> gateway
	for l_dy in (123, 88, 53):
		s_arrow(g, 98, l_dy, 168, 92, a_both=True)
	g.add(String(133, 128, 'MODBUS TCP', fontName='Sans', fontSize=7.5, textAnchor='middle'))
	# serial line to the bus
	s_arrow(g, 298, 86, 356, 86, a_both=True)
	g.add(String(327, 94, 'RS-485', fontName='Sans', fontSize=7.5, textAnchor='middle'))
	g.add(String(327, 72, 'MODBUS RTU', fontName='Sans', fontSize=7.5, textAnchor='middle'))
	# bus with devices
	g.add(Line(356, 86, 454, 86, strokeWidth=2))
	for i, l_x in enumerate((362, 396, 430)):
		g.add(Line(l_x + 10, 86, l_x + 10, 64, strokeWidth=1))
		s_box(g, l_x, 38, 22, 26, [str(i + 1)], 8)
	g.add(String(405, 20, a_L['dia_slaves'], fontName='SerifI', fontSize=8, textAnchor='middle'))
	return l_d

def dia_lifecycle (a_L):
	"""A request lifecycle with the exception path."""
	l_d = Drawing(460, 210)
	g = Group(); l_d.add(g)
	s_box(g, 4,   150, 100, 40, [a_L['dia_client'], 'MODBUS TCP'], 8)
	s_box(g, 150, 150, 112, 40, [a_L['dia_recv'], 'MBAP'], 8)
	s_box(g, 308, 150, 118, 40, [a_L['dia_tx'], '+ CRC16'], 8)
	s_box(g, 308, 78,  118, 40, [a_L['dia_rx'], 't3.5 / ' + a_L['dia_tmo']], 8)
	s_box(g, 150, 78,  112, 40, [a_L['dia_check'], 'CRC16'], 8)
	s_box(g, 4,   78,  100, 40, [a_L['dia_send'], 'MODBUS TCP'], 8)
	s_box(g, 150, 12,  276, 34, [a_L['dia_exc']], 8, colors.Color(1, 0.92, 0.88))
	s_arrow(g, 104, 170, 150, 170)
	s_arrow(g, 262, 170, 308, 170)
	s_arrow(g, 367, 150, 367, 118)
	s_arrow(g, 308, 98,  262, 98)
	s_arrow(g, 150, 98,  104, 98)
	# error path: rx/check -> exception -> send
	s_arrow(g, 367, 78, 367, 46)
	s_arrow(g, 206, 78, 206, 46)
	s_arrow(g, 150, 29, 54, 29)
	g.add(Line(54, 29, 54, 78, strokeWidth=1.2))
	s_arrow(g, 54, 60, 54, 78)
	g.add(String(230, 196, a_L['dia_okpath'], fontName='SerifI', fontSize=8, textAnchor='middle'))
	g.add(String(230, 2,  a_L['dia_errpath'], fontName='SerifI', fontSize=8, textAnchor='middle'))
	return l_d

def dia_ts_regs (a_L):
	"""The Time Stamp registers layout R0..R8."""
	l_d = Drawing(460, 96)
	g = Group(); l_d.add(g)
	l_x = 6
	l_lbl = ['R0', 'R1', 'R2', 'R3', 'R4', 'R5', 'R6', 'R7', 'R8']
	l_bit = ['63..48', '47..32', '31..16', '15..0', '63..48', '47..32', '31..16', '15..0', '']
	for i in range(9):
		l_fill = colors.Color(0.88, 0.92, 1) if i < 4 else (colors.Color(0.88, 1, 0.9) if i < 8 else colors.Color(1, 0.97, 0.85))
		s_box(g, l_x, 42, 46, 30, [l_lbl[i]], 9, l_fill)
		if l_bit[i]:
			g.add(String(l_x + 23, 30, l_bit[i], fontName='Mono', fontSize=6.6, textAnchor='middle'))
		l_x += 50
	g.add(String(6 + 2*50 - 2,   84, a_L['dia_secs'],  fontName='Sans', fontSize=8, textAnchor='middle'))
	g.add(String(6 + 6*50 - 2,   84, a_L['dia_nsecs'], fontName='Sans', fontSize=8, textAnchor='middle'))
	g.add(String(6 + 8*50 + 23,  84, a_L['dia_tz'],    fontName='Sans', fontSize=8, textAnchor='middle'))
	g.add(String(230, 8, a_L['dia_assemble'], fontName='Mono', fontSize=8, textAnchor='middle'))
	return l_d

def dia_msg_anatomy (a_L):
	"""The anatomy of a log message line."""
	l_d = Drawing(460, 110)
	g = Group(); l_d.add(g)
	l_msg = '%T2R-E-CRC16ERR, [#4:</dev/ttyUSB0>] RTU CRC16 check error ...'
	g.add(String(20, 62, l_msg, fontName='Mono', fontSize=9))
	# underline pieces: %T2R(4ch) -E(2) -CRC16ERR(9)
	l_cw = 5.42						# DejaVu Sans Mono advance at 9pt
	l_x0 = 20
	for (l_off, l_len, l_tx, l_lx) in (
			(0, 4,  a_L['dia_fac'], 30),
			(4, 2,  a_L['dia_sev'], 150),
			(6, 9,  a_L['dia_code'], 290)):
		l_a = l_x0 + l_off*l_cw
		l_b = l_a + l_len*l_cw
		g.add(Line(l_a, 58, l_b, 58, strokeWidth=1.4))
		g.add(Line((l_a + l_b)/2, 58, (l_a + l_b)/2, 44, strokeWidth=0.9))
		g.add(Line((l_a + l_b)/2, 44, l_lx, 36, strokeWidth=0.9))
		g.add(String(l_lx, 26, l_tx, fontName='Sans', fontSize=8, textAnchor='middle'))
	g.add(String(20, 88, a_L['dia_msgline'], fontName='SerifI', fontSize=8.5))
	return l_d

# ----------------------------------------------------------------------------------------------
#	The document template: VSI-style running heads, roman front matter / arabic body
# ----------------------------------------------------------------------------------------------
def s_roman (a_n):
	l_map = [(10, 'x'), (9, 'ix'), (5, 'v'), (4, 'iv'), (1, 'i')]
	l_out = ''
	while a_n > 0:
		for l_v, l_s in l_map:
			if a_n >= l_v:
				l_out += l_s; a_n -= l_v; break
	return l_out

class GuideDoc (BaseDocTemplate):
	def __init__ (self, a_fn, a_L, **kw):
		super().__init__(a_fn, pagesize=A4, leftMargin=LM, rightMargin=RM,
			topMargin=TM, bottomMargin=BM, **kw)
		self.m_L = a_L
		self.m_bodypage = 10**9			# The first arabic (body) page, found at pass 1
		l_frame = Frame(LM, BM, PAGE_W - LM - RM, PAGE_H - TM - BM, id='main')
		self.addPageTemplates([
			PageTemplate(id='Title', frames=[l_frame], onPage=self.s_pg_title),
			PageTemplate(id='Front', frames=[l_frame], onPage=self.s_pg_front),
			PageTemplate(id='Body',  frames=[l_frame], onPage=self.s_pg_body),
		])

	def s_head (self, a_cv):
		a_cv.setFont('Serif', 8.5)
		a_cv.drawCentredString(PAGE_W/2, PAGE_H - 14*mm, self.m_L['runhead'])
		a_cv.setLineWidth(0.5)
		a_cv.line(LM, PAGE_H - 16*mm, PAGE_W - RM, PAGE_H - 16*mm)

	def s_pg_title (self, a_cv, a_doc):
		pass

	def s_pg_front (self, a_cv, a_doc):
		self.s_head(a_cv)
		a_cv.setFont('Serif', 9)
		a_cv.drawCentredString(PAGE_W/2, 12*mm, s_roman(a_doc.page))

	def s_pg_body (self, a_cv, a_doc):
		if a_doc.page < self.m_bodypage:
			self.m_bodypage = a_doc.page
		self.s_head(a_cv)
		a_cv.setFont('Serif', 9)
		a_cv.drawCentredString(PAGE_W/2, 12*mm, str(a_doc.page - self.m_bodypage + 1))

	def afterFlowable (self, a_fl):
		if isinstance(a_fl, Paragraph) and a_fl.style.name in ('chap', 'sec'):
			l_lvl = 0 if a_fl.style.name == 'chap' else 1
			l_txt = a_fl.getPlainText()

			if l_txt == self.m_L['toc_h']:			# The Contents itself is not listed
				return

			l_key = 'k' + str(abs(hash(l_txt)) % 10**8)
			self.canv.bookmarkPage(l_key)
			self.notify('TOCEntry', (l_lvl, l_txt, self.page, l_key))

# ----------------------------------------------------------------------------------------------
#	The content, parameterized by the language pack
# ----------------------------------------------------------------------------------------------
def build (a_L, a_out):
	l_doc = GuideDoc(a_out, a_L, title=a_L['doctitle'], author=ORG)
	l_st = []

	# --- The title page (the VSI canon) ---
	l_st.append(Spacer(1, 30*mm))
	l_st.append(Paragraph('mbusgw-t2r', S['title1']))
	l_st.append(Paragraph(a_L['doctitle'], S['title2']))
	l_st.append(Spacer(1, 18*mm))
	for l_k, l_v in a_L['titleblock']:
		l_st.append(Paragraph('<b>%s</b> %s' % (l_k, l_v), S['tmeta']))
	l_st.append(Spacer(1, 42*mm))
	l_st.append(Paragraph(ORG, S['tmeta']))
	l_st.append(NextPageTemplate('Front'))
	l_st.append(PageBreak())

	# --- The legal page ---
	l_st.append(Paragraph(a_L['runhead'], S['legal']))
	l_st.append(Spacer(1, 8*mm))
	l_st.append(Paragraph(a_L['copyright'], S['legal']))
	l_st.append(Paragraph('<b>%s</b>' % a_L['legal_h'], S['legal']))
	for l_p in a_L['legal']:
		l_st.append(Paragraph(l_p, S['legal']))
	l_st.append(PageBreak())

	# --- The table of contents ---
	l_toc = TableOfContents()
	l_toc.levelStyles = [S['toc0'], S['toc1']]
	l_toc.dotsMinLevel = 0

	#
	# The displayed page number follows the VSI canon: the roman numerals for the front
	# matter, the arabic ones (restarted from 1) for the body. The body start page becomes
	# known after the first pass of multiBuild() and is applied by the subsequent passes.
	#
	def s_pgfmt (a_p):
		if l_doc.m_bodypage >= 10**9:
			return	str(a_p)
		if a_p >= l_doc.m_bodypage:
			return	str(a_p - l_doc.m_bodypage + 1)
		return	s_roman(a_p)

	l_toc.formatter = s_pgfmt
	l_st.append(Paragraph(a_L['toc_h'], S['chap']))
	l_st.append(l_toc)
	l_st.append(PageBreak())

	# --- The Preface (numbered subsections, per the VSI canon) ---
	l_st.append(Paragraph(a_L['preface_h'], S['chap']))
	for l_n, (l_h, l_paras) in enumerate(a_L['preface'], 1):
		l_st.append(Paragraph('%d. %s' % (l_n, l_h), S['sec']))
		for l_p in l_paras:
			l_st.append(Paragraph(l_p, S['body']))
	l_st.append(Paragraph('%d. %s' % (len(a_L['preface']) + 1, a_L['conv_h']), S['sec']))
	l_st.append(tbl([a_L['conv_c1'], a_L['conv_c2']], a_L['conventions'],
		[38*mm, 125*mm], a_mono_cols=(0,)))
	l_st.append(NextPageTemplate('Body'))
	l_st.append(PageBreak())

	# --- The chapters ---
	l_fig = [0]
	def fig (a_dia, a_cap):
		l_fig[0] += 1
		return KeepTogether([Spacer(1, 3*mm), a_dia,
			Paragraph(a_L['fig_w'] % l_fig[0] + a_cap, S['caption'])])

	for l_ch in a_L['chapters'](fig):
		for l_el in l_ch:
			l_st.append(l_el)
		l_st.append(PageBreak())
	del l_st[-1]

	l_doc.multiBuild(l_st)

# ----------------------------------------------------------------------------------------------
#	Helper constructors used by the language packs
# ----------------------------------------------------------------------------------------------
def P  (a_t): return Paragraph(a_t, S['body'])
def PI (a_t): return Paragraph(a_t, S['bodyi'])
def C  (a_t): return Preformatted(a_t, S['code'])
def H1 (a_t): return Paragraph(a_t, S['chap'])
def H2 (a_t): return Paragraph(a_t, S['sec'])
def H3 (a_t): return Paragraph(a_t, S['subsec'])

# ----------------------------------------------------------------------------------------------
#	The ENGLISH language pack
# ----------------------------------------------------------------------------------------------
def s_pack_en ():
	L = {}
	L['doctitle'] = 'User Guide'
	L['runhead']  = 'mbusgw-t2r User Guide'
	L['titleblock'] = [
		('Document Number:', 'DO-T2RUG-EN-01A'),
		('Publication Date:', PUBDATE_EN),
		('Revision Update Information:', 'This is a new manual.'),
		('Operating System:', 'Linux (glibc), kernel 4.x or later'),
		('Software Version:', 'mbusgw-t2r ' + VERSION),
	]
	L['copyright'] = 'Copyright © 2026 %s' % ORG
	L['legal_h'] = 'Legal Notice'
	L['legal'] = [
		'The information contained herein is subject to change without notice. '
		'%s shall not be liable for technical or editorial errors or omissions contained herein.' % ORG,
		'The software described in this manual is distributed in the hope that it will be useful, '
		'but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS '
		'FOR A PARTICULAR PURPOSE.',
		'MODBUS is a registered trademark of Schneider Electric USA, Inc. '
		'Linux is a registered trademark of Linus Torvalds. '
		'UNIX is a registered trademark of The Open Group.',
	]
	L['toc_h'] = 'Contents'
	L['preface_h'] = 'Preface'
	L['preface'] = [
		('About This Manual', [
			'This manual describes the configuration, the operation and the troubleshooting of '
			'<b>mbusgw-t2r</b> --- a gateway which translates MODBUS TCP requests coming from the '
			'network into MODBUS RTU transactions on a serial (RS-485 or RS-232) line, and returns '
			'the answers back.']),
		('Intended Audience', [
			'This manual is written for a reader with no prior MODBUS or Linux administration '
			'experience: every step is spelled out and nothing is assumed. An experienced engineer '
			'may go straight to Chapter 3 (the settings file) and Chapter 7 (the troubleshooting).']),
		('Document Structure', [
			'Chapter 1 explains what the gateway does. Chapter 2 is the pre-flight checklist. '
			'Chapter 3 describes the settings file key by key. Chapter 4 covers the start, the '
			'command line options and the first end-to-end test. Chapter 5 describes the built-in '
			'Time Stamp pseudo device. Chapter 6 teaches how to read the log. Chapter 7 is the '
			'troubleshooting reference: the symptom, the cause, the action, and the reference of '
			'all the message codes.']),
		('Related Documents', [
			'<i>README.md</i> --- the installation sequence (the StarLet package, the build, '
			'<font face="Mono">make install</font>). '
			'<i>MODBUS over Serial Line Specification and Implementation Guide</i> and '
			'<i>MODBUS Application Protocol Specification</i> --- the protocol itself, '
			'available from modbus.org.']),
	]
	L['conv_h'] = 'Conventions'
	L['conv_c1'] = 'Convention'; L['conv_c2'] = 'Meaning'
	L['conventions'] = [
		('Monospace type', 'Code examples, file names, commands and interactive screen displays.'),
		('%T2R-E-CODE',    'A message code as it appears in the gateway log; see Section 7.2.'),
		('<...>',          'A placeholder to be replaced by an actual value, e.g. a device name.'),
		('$',              'The shell prompt of an unprivileged user; do not type it.'),
		('Ctrl/C',         'Hold down the key labeled Ctrl while you press the key C.'),
	]
	# Diagram labels
	L.update({
		'dia_client': 'TCP client', 'dia_tcpside': 'SCADA, mbpoll, scripts ...',
		'dia_queue': '(the request queue)', 'dia_slaves': 'RTU devices (slaves)',
		'dia_recv': 'Receive and check', 'dia_tx': 'Transmit to the line',
		'dia_rx': 'Receive the answer', 'dia_tmo': 'timeout', 'dia_check': 'Check the answer',
		'dia_send': 'Answer the client', 'dia_exc': 'Form a MODBUS exception (EXCRPT is logged)',
		'dia_okpath': 'The normal path', 'dia_errpath': 'The error path: any failure below becomes an exception answer',
		'dia_secs': 'Seconds (64 bits, the MSW first)', 'dia_nsecs': 'Nanoseconds (64 bits)',
		'dia_tz': 'TZ, min', 'dia_assemble': 'seconds = (R0<<48) | (R1<<32) | (R2<<16) | R3',
		'dia_fac': 'the facility', 'dia_sev': 'the severity: S I W E F', 'dia_code': 'the message code (Section 7.2)',
		'dia_msgline': 'Every important event is one line with a code:',
	})
	L['fig_w'] = 'Figure %d.  '

	def chapters (fig):
		return [
		# --- Chapter 1 ---
		[H1('Chapter 1.  What the Gateway Does'),
		 P('You have a device (an electricity meter, a temperature controller, a PLC ...) which '
		   'talks <b>MODBUS RTU</b> over a serial line. And you have a program (a SCADA, a script, '
		   '<font face="Mono">mbpoll</font> ...) which talks <b>MODBUS TCP</b> over the network. '
		   'The gateway sits between them and translates, as Figure 1 shows.'),
		 fig(dia_architecture(L), 'The data path of the gateway'),
		 P('Several TCP clients may connect at the same time: the gateway queues them so that '
		   'only one request at a time goes to the serial line --- an RTU line physically cannot '
		   'carry two transactions at once.'),
		 P('Several devices may share one RS-485 line. They are told apart by the <i>slave '
		   'address</i> inside the MODBUS request itself; the gateway does not need to know '
		   'their addresses at all.')],
		# --- Chapter 2 ---
		[H1('Chapter 2.  Before You Start'),
		 P('Walk this checklist once --- it saves an hour of guessing later.'),
		 P('<b>1.</b> The gateway is installed (see <i>README.md</i>). After the installation you have '
		   'the program <font face="Mono">/usr/local/sbin/mbusgw-t2r</font> and the settings file '
		   '<font face="Mono">/usr/local/etc/mbusgw-t2r/modbus-t2r_settings.conf</font>.'),
		 P('<b>2.</b> You know which serial port the device is connected to. On Linux it is a file like '
		   '<font face="Mono">/dev/ttyS0</font> or <font face="Mono">/dev/ttyUSB0</font>. If unsure, '
		   'plug the USB adapter out and in, then run:'),
		 C('$ dmesg | tail'),
		 P('<b>3.</b> You know the line parameters of the device: the speed, the data bits, the parity '
		   'and the stop bits. They are in the device manual; the most common set is '
		   '<font face="Mono">9600, 8, N, 1</font>.'),
		 P('<b>4.</b> Your user may open the port. Check the group of the port file:'),
		 C('$ ls -l /dev/ttyUSB0\n'
		   'crw-rw---- 1 root dialout 188, 0 ... /dev/ttyUSB0'),
		 P('If the group is <font face="Mono">dialout</font>, add yourself to it and re-login '
		   '(or simply run the gateway with <font face="Mono">sudo</font>):'),
		 C('$ sudo usermod -a -G dialout $USER')],
		# --- Chapter 3 ---
		[H1('Chapter 3.  The Settings File'),
		 P('The settings file has two sections: <font face="Mono">serials</font> (the serial '
		   'lines) and <font face="Mono">listeners</font> (the TCP ports). A minimal working '
		   'example:'),
		 C('serials = (\n'
		   '\t{\tdevice = "/dev/ttyUSB0";\n'
		   '\t\tchars  = "9600, 8, N, 1";\n'
		   '\t\tiotmo  = 1000;\n'
		   '\t}\n'
		   ');\n\n'
		   'listeners = (\n'
		   '\t{\tbind   = "TCP:0.0.0.0:502";\n'
		   '\t\ttarget = "/dev/ttyUSB0";\n'
		   '\t\tconnlm = 8;\n'
		   '\t}\n'
		   ');'),
		 P('Read it as: <i>"open the port /dev/ttyUSB0 at 9600-8-N-1 and wait for an answer up '
		   'to 1000 ms; listen for TCP clients on every interface, port 502, and forward them '
		   'to that port"</i>.'),
		 H2('3.1.  The serials Section'),
		 P('One record per serial port. Table 3-1 lists every key.'),
		 tbl(['Key', 'Required', 'Meaning and the allowed values'], [
			('device', 'yes', 'The serial port file, e.g. /dev/ttyUSB0.'),
			('chars', 'yes', 'The line parameters: speed, data bits, parity, stop bits. '
				'Speed 50..4000000; data bits 5..8; parity N (none), E (even), O (odd); stop bits 1..2.'),
			('iotmo', 'no', 'How long to wait for the answer of the device, milliseconds. Default 1000.'),
			('rs485', 'no', '1 --- ask the kernel to drive the RS-485 direction control '
				'(only for the ports which support it). Default 0.'),
			('ts_enabled', 'no', '1 --- enable the Time Stamp pseudo device (Chapter 5). Default 0.'),
			('ts_unit', 'no', 'The slave address the pseudo device answers on. Default 135.'),
			('ts_fncode', 'no', 'The function code it answers to. Default 4.'),
			('ts_base_reg0', 'no', 'The first register of the time stamp. Default 135.'),
		 ], [30*mm, 18*mm, 115*mm], a_mono_cols=(0,)),
		 P('If a record is wrong, the gateway skips it and says why --- with the allowed range '
		   'right in the message:'),
		 C('%T2R-E:  [serial #00:</dev/ttyUSB0>] --- speed 31 baud is out of range [50..4000000]'),
		 H2('3.2.  The listeners Section'),
		 tbl(['Key', 'Required', 'Meaning and the allowed values'], [
			('bind', 'yes', 'Where to listen: TCP:&lt;IP address&gt;:&lt;port&gt;. The port is 1..65535; '
				'0.0.0.0 means every interface. UDP is not supported.'),
			('target', 'yes', 'Which serial port to forward to. Must match a device from serials '
				'exactly, character by character.'),
			('connlm', 'no', 'How many TCP clients may wait in the connection queue, e.g. 8.'),
			('iotmo', 'no', 'The network I/O timeout, milliseconds.'),
		 ], [30*mm, 18*mm, 115*mm], a_mono_cols=(0,)),
		 P('Port <b>502</b> is the standard MODBUS TCP port, but on Linux the ports below 1024 '
		   'need the root privileges. If you do not want to run as root, take e.g. '
		   '<font face="Mono">5502</font> and point your client there. Several listeners may '
		   'share one target: the gateway serializes their requests.')],
		# --- Chapter 4 ---
		[H1('Chapter 4.  Running the Gateway'),
		 P('By hand, in a terminal (the messages go straight to the screen):'),
		 C('$ /usr/local/sbin/mbusgw-t2r /settings=/usr/local/etc/mbusgw-t2r/modbus-t2r_settings.conf'),
		 tbl(['Option', 'Meaning'], [
			('/settings=&lt;file&gt;', 'The settings file (Chapter 3).'),
			('/trace', 'The verbose tracing: every request and answer is dumped in hex. '
				'Priceless at the first run.'),
			('/logfile=&lt;file&gt;', 'Write the log to a file instead of the screen.'),
			('/logsize=&lt;octets&gt;', 'Rotate the log file above this size.'),
		 ], [42*mm, 121*mm], a_mono_cols=(0,)),
		 P('A healthy start prints (shortened):'),
		 C('%T2R-I-REVISNF, Rev: T2R X.00-06/aarch64(built at ...) (REV: 00.06.00)\n'
		   '%T2R-I:  Added device #00 [</dev/ttyUSB0>, Chars: <9600, 8, N, 1>, ...] --- added\n'
		   '%T2R-I:  Added listener #00 [Target: </dev/ttyUSB0>, Net: <TCP:0.0.0.0:502>, ...] --- added\n'
		   '%T2R-S-DEVREADY, Device </dev/ttyUSB0> [9600 baud, answer tmo: 1000 msec, t3.5: 4010 usec] --- is ready\n'
		   '%T2R-S-LSNRRDY, [#3] Listener 0.0.0.0:502 [Target: </dev/ttyUSB0>] --- is ready'),
		 P('Two lines matter most: <b>DEVREADY</b> (the port is open) and <b>LSNRRDY</b> (the '
		   'TCP port is listening). If you see both, the gateway is up. Figure 2 shows what '
		   'happens to every request after that.'),
		 fig(dia_lifecycle(L), 'The lifecycle of one request'),
		 P('To stop the gateway press Ctrl/C once and wait a second. To toggle the tracing of a '
		   '<i>running</i> gateway without a restart:'),
		 C('$ kill -USR1 <pid>'),
		 H2('4.1.  The First Test with mbpoll'),
		 P('<font face="Mono">mbpoll</font> is a small free MODBUS client '
		   '(<font face="Mono">sudo apt install mbpoll</font>). Read 3 holding registers from '
		   'the device with the slave address 1:'),
		 C('$ mbpoll -a 1 -r 1 -c 3 -1 127.0.0.1 -p 502'),
		 P('If the numbers come back --- everything works, end to end.')],
		# --- Chapter 5 ---
		[H1('Chapter 5.  The Time Stamp Pseudo Device'),
		 P('The gateway can answer <i>by itself</i> --- without touching the line --- to a read '
		   'request sent to a special slave address. The answer is the current time of the '
		   'gateway host. This lets a SCADA read the precise time over the plain MODBUS.'),
		 P('With <font face="Mono">ts_enabled = 1; ts_unit = 135; ts_fncode = 4; '
		   'ts_base_reg0 = 135;</font> a request "read N input registers starting at 135 from '
		   'the slave 135" returns the registers of Figure 3.'),
		 fig(dia_ts_regs(L), 'The Time Stamp registers layout'),
		 tbl(['Registers requested', 'You get'], [
			('4', 'The seconds of the UNIX epoch, 64 bits, the most significant 16-bit word first.'),
			('8', 'Plus the nanoseconds, packed the same way into R4..R7.'),
			('9', 'Plus R8 = the local timezone offset, in signed minutes (e.g. Moscow = +180).'),
		 ], [40*mm, 123*mm], a_mono_cols=(0,)),
		 P('Any other register count in a TS request is answered with the MODBUS exception '
		   'ILLEGAL_DATA_ADDRESS --- this is on purpose.')],
		# --- Chapter 6 ---
		[H1('Chapter 6.  Reading the Log'),
		 fig(dia_msg_anatomy(L), 'The anatomy of a log message'),
		 P('The severity letter is: <b>S</b> --- success, <b>I</b> --- information, '
		   '<b>W</b> --- warning, <b>E</b> --- error, <b>F</b> --- fatal. The part '
		   '<font face="Mono">[#4:</font><font face="Mono">&lt;/dev/ttyUSB0&gt;]</font> means: '
		   'the file descriptor #4, this serial port.'),
		 P('Grep the log by the code --- this is exactly why the codes exist:'),
		 C('$ grep CRC16ERR /var/log/mbusgw-t2r.log')],
		# --- Chapter 7 ---
		[H1('Chapter 7.  Troubleshooting'),
		 P('<b>The golden rule: run with /trace and read the log. The gateway always says what '
		   'it dislikes.</b>'),
		 H2('7.1.  The Symptom, the Cause, the Action'),
		 tbl(['You see', 'It means / what to do'], [
			('%T2R-E-DEVOPNERR, ... errno: 2', 'The port file does not exist: the adapter is unplugged '
				'or the name is wrong. Run dmesg | tail after plugging; fix device in the settings.'),
			('%T2R-E-DEVOPNERR, ... errno: 13', 'Permission denied. Run with sudo, or add yourself '
				'to the dialout group.'),
			('%T2R-E-DEVOPNERR, ... errno: 16', 'The port is busy. Find the holder: '
				'sudo fuser /dev/ttyUSB0, and stop it.'),
			('%T2R-E-NOANSWER, ...', 'The request went out, nothing came back: 1) a wrong slave '
				'address in the client request; 2) the wiring (swap A/B of RS-485); 3) a wrong '
				'speed/parity --- re-check chars; 4) the device is off.'),
			('%T2R-E-CRC16ERR, ...', 'The octets arrive damaged: 1) chars does not match the device '
				'(the most common!); 2) the line noise --- the grounding, the termination, the cable '
				'length; 3) two masters on one line.'),
			('%T2R-E-BADFRAME, ...', 'Something arrived, but it is not a sane MODBUS frame: usually '
				'the same causes as CRC16ERR; 400+ octets of garbage mean a foreign, non-MODBUS '
				'device on the line.'),
			('%T2R-W-FRAMETMO, ...', 'The answer started but never finished: a very slow or hanging '
				'device. Raise iotmo; check the cable.'),
			('%T2R-W-EXCRPT, ... (SERVER_DEVICE_FAILURE)', 'The gateway told the client "the serial '
				'side failed". The real reason is in the previous log line (NOANSWER, CRC16ERR ...).'),
			('%T2R-E-LSNRERR, ... errno: 98', 'The TCP port is taken: a second gateway instance, or '
				'another program. Find it: sudo ss -tlnp | grep 502.'),
			('%T2R-E-LSNRERR, ... errno: 13', 'The ports below 1024 need root. Run with sudo, or '
				'take a port above 1024 (e.g. 5502).'),
			('... out of range [a..b]', 'A settings value is out of its range; the allowed range is '
				'right in the message. Fix the value.'),
			('No serials has been defined!', 'Not a single serials record survived the validation. '
				'Read the error lines above --- each rejected record says why.'),
			('The data is wrong or shifted', 'The byte order confusion on the client side: the MODBUS '
				'registers are the big-endian 16-bit words; check how the client assembles the '
				'32/64-bit values.'),
		 ], [52*mm, 111*mm], a_mono_cols=(0,)),
		 H2('7.2.  The Message Codes Reference'),
		 tbl(['Code', 'Sev', 'When it appears'], [
			('REVISNF', 'I', 'At the start: the program version. Quote it when asking for help.'),
			('DEVREADY', 'S', 'The serial port is open; the actual baud, the answer timeout and '
				'the t3.5 interval are shown.'),
			('LSNRRDY', 'S', 'The TCP port is listening; the address, the port and the target '
				'device are shown.'),
			('NETCONN', 'S', 'A TCP client has connected; its address:port and the listener '
				'are shown.'),
			('NETDISCN', 'S', 'A TCP client has disconnected; its address:port are shown.'),
			('DEVOPNERR', 'E', 'The serial port cannot be opened; errno says why (2 = no such '
				'file, 13 = the permissions, 16 = busy).'),
			('LSNRERR', 'E', 'bind()/listen() failed for a TCP port; errno says why (98 = taken, '
				'13 = the privileges).'),
			('NOANSWER', 'E', 'The device did not answer within the iotmo timeout.'),
			('FRAMETMO', 'W', 'The answer began but was not completed within the timeout.'),
			('BADFRAME', 'E', 'The received frame length is outside the sane [5..256] octets range.'),
			('CRC16ERR', 'E', 'The checksum of the received frame does not match; both values '
				'are shown.'),
			('EXCRPT', 'W', 'A MODBUS exception is returned to the TCP client; the code and its '
				'name are shown.'),
			('EXITST', 'I', 'The gateway exits; the exit flag and the final status are shown.'),
		 ], [26*mm, 10*mm, 127*mm], a_mono_cols=(0,)),
		 H2('7.3.  If Nothing Helps'),
		 P('Collect and attach to your question: 1) the full start-up log with /trace (from '
		   'REVISNF to the first error); 2) your settings file; 3) the output of '
		   '<font face="Mono">ls -l &lt;device&gt;</font> and <font face="Mono">dmesg | '
		   'tail -20</font>; 4) the exact model of the RTU device and its documented line '
		   'parameters. With these four things the problem is almost always visible at a glance.')],
		]
	L['chapters'] = chapters
	return L

# ----------------------------------------------------------------------------------------------
#	The RUSSIAN language pack
# ----------------------------------------------------------------------------------------------
def s_pack_ru ():
	L = {}
	L['doctitle'] = 'Руководство пользователя'
	L['runhead']  = 'mbusgw-t2r — Руководство пользователя'
	L['titleblock'] = [
		('Номер документа:', 'DO-T2RUG-RU-01A'),
		('Дата публикации:', PUBDATE_RU),
		('Сведения о ревизии:', 'Это новое руководство.'),
		('Операционная система:', 'Linux (glibc), ядро 4.x и новее'),
		('Версия программы:', 'mbusgw-t2r ' + VERSION),
	]
	L['copyright'] = 'Copyright © 2026 %s' % ORG
	L['legal_h'] = 'Правовая информация'
	L['legal'] = [
		'Сведения в настоящем документе могут быть изменены без предварительного уведомления. '
		'%s не несёт ответственности за технические или редакционные ошибки и пропуски в настоящем документе.' % ORG,
		'Программа, описанная в настоящем руководстве, распространяется в надежде, что она будет полезной, '
		'но БЕЗ КАКИХ-ЛИБО ГАРАНТИЙ, в том числе без подразумеваемых гарантий товарной пригодности '
		'и пригодности для конкретной цели.',
		'MODBUS — зарегистрированный товарный знак Schneider Electric USA, Inc. '
		'Linux — зарегистрированный товарный знак Линуса Торвальдса. '
		'UNIX — зарегистрированный товарный знак The Open Group.',
	]
	L['toc_h'] = 'Содержание'
	L['preface_h'] = 'Предисловие'
	L['preface'] = [
		('О настоящем руководстве', [
			'Настоящее руководство описывает настройку, эксплуатацию и поиск неисправностей '
			'<b>mbusgw-t2r</b> — шлюза, который переводит запросы MODBUS TCP, приходящие из сети, '
			'в транзакции MODBUS RTU на последовательной линии (RS-485 или RS-232) и возвращает '
			'ответы обратно.']),
		('Для кого написано', [
			'Руководство рассчитано на читателя без опыта работы с MODBUS и администрирования '
			'Linux: каждый шаг расписан, ничего не подразумевается. Опытный инженер может сразу '
			'перейти к главе 3 (файл настроек) и главе 7 (поиск неисправностей).']),
		('Структура документа', [
			'Глава 1 объясняет, что делает шлюз. Глава 2 — контрольный список перед запуском. '
			'Глава 3 описывает файл настроек ключ за ключом. Глава 4 — запуск, опции командной '
			'строки и первая сквозная проверка. Глава 5 — встроенное псевдоустройство меток '
			'времени. Глава 6 учит читать журнал. Глава 7 — справочник по поиску неисправностей: '
			'симптом, причина, действие, и справочник всех кодов сообщений.']),
		('Смежные документы', [
			'<i>README.md</i> — последовательность установки (пакет StarLet, сборка, '
			'<font face="Mono">make install</font>). '
			'<i>MODBUS over Serial Line Specification and Implementation Guide</i> и '
			'<i>MODBUS Application Protocol Specification</i> — сам протокол, доступны на modbus.org.']),
	]
	L['conv_h'] = 'Соглашения'
	L['conv_c1'] = 'Обозначение'; L['conv_c2'] = 'Значение'
	L['conventions'] = [
		('Моноширинный', 'Примеры кода, имена файлов, команды и экранный вывод.'),
		('%T2R-E-КОД',   'Код сообщения, как он выглядит в журнале шлюза; см. раздел 7.2.'),
		('<...>',        'Место подстановки фактического значения, например имени устройства.'),
		('$',            'Приглашение оболочки непривилегированного пользователя; вводить его не нужно.'),
		('Ctrl/C',       'Удерживая клавишу Ctrl, нажмите клавишу C.'),
	]
	L.update({
		'dia_client': 'TCP-клиент', 'dia_tcpside': 'SCADA, mbpoll, скрипты ...',
		'dia_queue': '(очередь запросов)', 'dia_slaves': 'RTU-устройства (ведомые)',
		'dia_recv': 'Приём и проверка', 'dia_tx': 'Передача в линию',
		'dia_rx': 'Приём ответа', 'dia_tmo': 'таймаут', 'dia_check': 'Проверка ответа',
		'dia_send': 'Ответ клиенту', 'dia_exc': 'Формирование MODBUS-исключения (в журнал идёт EXCRPT)',
		'dia_okpath': 'Нормальный путь', 'dia_errpath': 'Путь ошибки: любой сбой внизу превращается в ответ-исключение',
		'dia_secs': 'Секунды (64 бита, старшее слово первым)', 'dia_nsecs': 'Наносекунды (64 бита)',
		'dia_tz': 'Пояс, мин', 'dia_assemble': 'секунды = (R0<<48) | (R1<<32) | (R2<<16) | R3',
		'dia_fac': 'подсистема', 'dia_sev': 'серьёзность: S I W E F', 'dia_code': 'код сообщения (раздел 7.2)',
		'dia_msgline': 'Каждое важное событие — одна строка с кодом:',
	})
	L['fig_w'] = 'Рисунок %d.  '

	def chapters (fig):
		return [
		[H1('Глава 1.  Что делает шлюз'),
		 P('У вас есть устройство (счётчик электроэнергии, терморегулятор, ПЛК ...), которое '
		   'говорит на <b>MODBUS RTU</b> по последовательной линии. И есть программа (SCADA, '
		   'скрипт, <font face="Mono">mbpoll</font> ...), которая говорит на <b>MODBUS TCP</b> '
		   'по сети. Шлюз стоит между ними и переводит, как показано на рисунке 1.'),
		 fig(dia_architecture(L), 'Путь данных через шлюз'),
		 P('Несколько TCP-клиентов могут подключаться одновременно: шлюз выстраивает их в '
		   'очередь, чтобы на линию уходил только один запрос за раз — линия RTU физически не '
		   'умеет нести две транзакции сразу.'),
		 P('На одной линии RS-485 может сидеть несколько устройств. Они различаются <i>адресом '
		   'ведомого</i> внутри самого MODBUS-запроса; шлюзу их адреса знать не нужно вовсе.')],
		[H1('Глава 2.  Перед началом'),
		 P('Пройдите этот список один раз — он сэкономит час гаданий потом.'),
		 P('<b>1.</b> Шлюз установлен (см. <i>README.md</i>). После установки у вас есть программа '
		   '<font face="Mono">/usr/local/sbin/mbusgw-t2r</font> и файл настроек '
		   '<font face="Mono">/usr/local/etc/mbusgw-t2r/modbus-t2r_settings.conf</font>.'),
		 P('<b>2.</b> Вы знаете, к какому последовательному порту подключено устройство. В Linux это '
		   'файл вида <font face="Mono">/dev/ttyS0</font> или <font face="Mono">/dev/ttyUSB0</font>. '
		   'Если не уверены — выдерните и вставьте USB-адаптер и выполните:'),
		 C('$ dmesg | tail'),
		 P('<b>3.</b> Вы знаете параметры линии устройства: скорость, биты данных, чётность, '
		   'стоп-биты. Они написаны в паспорте устройства; самый распространённый набор — '
		   '<font face="Mono">9600, 8, N, 1</font>.'),
		 P('<b>4.</b> Ваш пользователь имеет право открыть порт. Проверьте группу файла порта:'),
		 C('$ ls -l /dev/ttyUSB0\n'
		   'crw-rw---- 1 root dialout 188, 0 ... /dev/ttyUSB0'),
		 P('Если группа <font face="Mono">dialout</font> — добавьте себя в неё и перезайдите в '
		   'систему (либо просто запускайте шлюз через <font face="Mono">sudo</font>):'),
		 C('$ sudo usermod -a -G dialout $USER')],
		[H1('Глава 3.  Файл настроек'),
		 P('В файле два раздела: <font face="Mono">serials</font> (последовательные линии) и '
		   '<font face="Mono">listeners</font> (TCP-порты). Минимальный рабочий пример:'),
		 C('serials = (\n'
		   '\t{\tdevice = "/dev/ttyUSB0";\n'
		   '\t\tchars  = "9600, 8, N, 1";\n'
		   '\t\tiotmo  = 1000;\n'
		   '\t}\n'
		   ');\n\n'
		   'listeners = (\n'
		   '\t{\tbind   = "TCP:0.0.0.0:502";\n'
		   '\t\ttarget = "/dev/ttyUSB0";\n'
		   '\t\tconnlm = 8;\n'
		   '\t}\n'
		   ');'),
		 P('Читается так: <i>«открой порт /dev/ttyUSB0 на 9600-8-N-1 и жди ответа до 1000 мс; '
		   'слушай TCP-клиентов на всех интерфейсах, порт 502, и пересылай их на этот порт»</i>.'),
		 H2('3.1.  Раздел serials'),
		 P('По одной записи на каждый последовательный порт. Таблица 3-1 перечисляет все ключи.'),
		 tbl(['Ключ', 'Обяз.', 'Значение и допустимые величины'], [
			('device', 'да', 'Файл последовательного порта, например /dev/ttyUSB0.'),
			('chars', 'да', 'Параметры линии: скорость, биты данных, чётность, стоп-биты. '
				'Скорость 50..4000000; биты данных 5..8; чётность N (нет), E (чётная), O (нечётная); '
				'стоп-биты 1..2.'),
			('iotmo', 'нет', 'Сколько ждать ответа устройства, миллисекунды. По умолчанию 1000.'),
			('rs485', 'нет', '1 — просить ядро управлять направлением RS-485 (только для портов, '
				'которые это умеют). По умолчанию 0.'),
			('ts_enabled', 'нет', '1 — включить псевдоустройство меток времени (глава 5). По умолчанию 0.'),
			('ts_unit', 'нет', 'Адрес ведомого, на который отвечает псевдоустройство. По умолчанию 135.'),
			('ts_fncode', 'нет', 'Код функции, на который оно отвечает. По умолчанию 4.'),
			('ts_base_reg0', 'нет', 'Первый регистр метки времени. По умолчанию 135.'),
		 ], [30*mm, 14*mm, 119*mm], a_mono_cols=(0,)),
		 P('Если запись неправильная, шлюз пропускает её и говорит почему — с допустимым '
		   'диапазоном прямо в сообщении:'),
		 C('%T2R-E:  [serial #00:</dev/ttyUSB0>] --- speed 31 baud is out of range [50..4000000]'),
		 H2('3.2.  Раздел listeners'),
		 tbl(['Ключ', 'Обяз.', 'Значение и допустимые величины'], [
			('bind', 'да', 'Где слушать: TCP:&lt;IP-адрес&gt;:&lt;порт&gt;. Порт 1..65535; '
				'0.0.0.0 — все интерфейсы. UDP не поддерживается.'),
			('target', 'да', 'На какой порт пересылать. Должен дословно, символ в символ, '
				'совпадать с device из serials.'),
			('connlm', 'нет', 'Сколько TCP-клиентов может ждать в очереди подключения, например 8.'),
			('iotmo', 'нет', 'Сетевой таймаут ввода-вывода, миллисекунды.'),
		 ], [30*mm, 14*mm, 119*mm], a_mono_cols=(0,)),
		 P('Порт <b>502</b> — стандартный порт MODBUS TCP, но в Linux порты ниже 1024 требуют '
		   'прав root. Не хотите работать под root — возьмите, например, '
		   '<font face="Mono">5502</font> и укажите его в клиенте. Несколько listeners могут '
		   'разделять один target: шлюз выстроит их запросы в очередь.')],
		[H1('Глава 4.  Запуск шлюза'),
		 P('Вручную, в терминале (сообщения идут прямо на экран):'),
		 C('$ /usr/local/sbin/mbusgw-t2r /settings=/usr/local/etc/mbusgw-t2r/modbus-t2r_settings.conf'),
		 tbl(['Опция', 'Смысл'], [
			('/settings=&lt;файл&gt;', 'Файл настроек (глава 3).'),
			('/trace', 'Подробная трассировка: каждый запрос и ответ печатается в hex. '
				'Бесценно при первом запуске.'),
			('/logfile=&lt;файл&gt;', 'Писать журнал в файл вместо экрана.'),
			('/logsize=&lt;октет&gt;', 'Ротировать файл журнала при превышении размера.'),
		 ], [42*mm, 121*mm], a_mono_cols=(0,)),
		 P('Здоровый запуск печатает (сокращённо):'),
		 C('%T2R-I-REVISNF, Rev: T2R X.00-06/aarch64(built at ...) (REV: 00.06.00)\n'
		   '%T2R-I:  Added device #00 [</dev/ttyUSB0>, Chars: <9600, 8, N, 1>, ...] --- added\n'
		   '%T2R-I:  Added listener #00 [Target: </dev/ttyUSB0>, Net: <TCP:0.0.0.0:502>, ...] --- added\n'
		   '%T2R-S-DEVREADY, Device </dev/ttyUSB0> [9600 baud, answer tmo: 1000 msec, t3.5: 4010 usec] --- is ready\n'
		   '%T2R-S-LSNRRDY, [#3] Listener 0.0.0.0:502 [Target: </dev/ttyUSB0>] --- is ready'),
		 P('Важнее всего две строки: <b>DEVREADY</b> (порт открыт) и <b>LSNRRDY</b> (TCP-порт '
		   'слушает). Видите обе — шлюз работает. Рисунок 2 показывает, что происходит дальше '
		   'с каждым запросом.'),
		 fig(dia_lifecycle(L), 'Жизненный цикл одного запроса'),
		 P('Остановка: один раз Ctrl/C и секунда ожидания. Переключить трассировку у <i>уже '
		   'работающего</i> шлюза без перезапуска:'),
		 C('$ kill -USR1 <pid>'),
		 H2('4.1.  Первая проверка с mbpoll'),
		 P('<font face="Mono">mbpoll</font> — маленький свободный MODBUS-клиент '
		   '(<font face="Mono">sudo apt install mbpoll</font>). Читаем 3 регистра хранения с '
		   'устройства с адресом 1:'),
		 C('$ mbpoll -a 1 -r 1 -c 3 -1 127.0.0.1 -p 502'),
		 P('Если вернулись числа — всё работает, из конца в конец.')],
		[H1('Глава 5.  Псевдоустройство меток времени'),
		 P('Шлюз умеет отвечать <i>сам</i> — не трогая линию — на запрос чтения, посланный на '
		   'специальный адрес ведомого. Ответ — текущее время машины, на которой работает шлюз. '
		   'Так SCADA может читать точное время по обычному MODBUS.'),
		 P('При <font face="Mono">ts_enabled = 1; ts_unit = 135; ts_fncode = 4; '
		   'ts_base_reg0 = 135;</font> запрос «прочитай N входных регистров начиная со 135-го '
		   'у ведомого 135» возвращает регистры рисунка 3.'),
		 fig(dia_ts_regs(L), 'Раскладка регистров метки времени'),
		 tbl(['Запрошено регистров', 'Что получаете'], [
			('4', 'Секунды эпохи UNIX, 64 бита, старшее 16-битное слово первым.'),
			('8', 'Плюс наносекунды, упакованные так же в R4..R7.'),
			('9', 'Плюс R8 — смещение местного часового пояса, в знаковых минутах (Москва = +180).'),
		 ], [40*mm, 123*mm], a_mono_cols=(0,)),
		 P('Любое другое число регистров в TS-запросе получает MODBUS-исключение '
		   'ILLEGAL_DATA_ADDRESS — это сделано намеренно.')],
		[H1('Глава 6.  Как читать журнал'),
		 fig(dia_msg_anatomy(L), 'Анатомия сообщения журнала'),
		 P('Буква серьёзности: <b>S</b> — успех, <b>I</b> — информация, <b>W</b> — '
		   'предупреждение, <b>E</b> — ошибка, <b>F</b> — фатально. Часть '
		   '<font face="Mono">[#4:</font><font face="Mono">&lt;/dev/ttyUSB0&gt;]</font> '
		   'означает: файловый дескриптор #4, вот этот последовательный порт.'),
		 P('Ищите в журнале по коду — именно для этого коды и существуют:'),
		 C('$ grep CRC16ERR /var/log/mbusgw-t2r.log')],
		[H1('Глава 7.  Поиск неисправностей'),
		 P('<b>Золотое правило: запустите с /trace и читайте журнал. Шлюз всегда говорит, что '
		   'именно ему не нравится.</b>'),
		 H2('7.1.  Симптом, причина, действие'),
		 tbl(['Вы видите', 'Что это значит / что делать'], [
			('%T2R-E-DEVOPNERR, ... errno: 2', 'Файла порта нет: адаптер выдернут или имя '
				'неверное. Выполните dmesg | tail после втыкания; поправьте device в настройках.'),
			('%T2R-E-DEVOPNERR, ... errno: 13', 'Нет прав доступа. Запустите под sudo или '
				'добавьте себя в группу dialout.'),
			('%T2R-E-DEVOPNERR, ... errno: 16', 'Порт занят. Найдите держателя: '
				'sudo fuser /dev/ttyUSB0, и остановите его.'),
			('%T2R-E-NOANSWER, ...', 'Запрос ушёл, ответа нет: 1) неверный адрес ведомого в '
				'запросе клиента; 2) провода (поменяйте A/B у RS-485); 3) не та скорость/чётность '
				'— перепроверьте chars; 4) устройство выключено.'),
			('%T2R-E-CRC16ERR, ...', 'Байты приходят повреждёнными: 1) chars не совпадает с '
				'устройством (самое частое!); 2) помехи — земля, терминаторы, длина кабеля; '
				'3) два мастера на одной линии.'),
			('%T2R-E-BADFRAME, ...', 'Что-то пришло, но это не осмысленный MODBUS-кадр: обычно '
				'те же причины, что у CRC16ERR; 400+ октетов мусора — на линии чужое, '
				'не-MODBUS устройство.'),
			('%T2R-W-FRAMETMO, ...', 'Ответ начался, но не завершился: очень медленное или '
				'зависшее устройство. Увеличьте iotmo; проверьте кабель.'),
			('%T2R-W-EXCRPT, ... (SERVER_DEVICE_FAILURE)', 'Шлюз сообщил клиенту «на '
				'последовательной стороне сбой». Настоящая причина — в предыдущей строке журнала '
				'(NOANSWER, CRC16ERR ...).'),
			('%T2R-E-LSNRERR, ... errno: 98', 'TCP-порт занят: второй экземпляр шлюза или другая '
				'программа. Найдите: sudo ss -tlnp | grep 502.'),
			('%T2R-E-LSNRERR, ... errno: 13', 'Порты ниже 1024 требуют root. Запустите под sudo '
				'или возьмите порт выше 1024 (например 5502).'),
			('... out of range [a..b]', 'Значение в настройках вне диапазона; допустимый диапазон '
				'написан прямо в сообщении. Исправьте значение.'),
			('No serials has been defined!', 'Ни одна запись serials не прошла проверку. Читайте '
				'строки ошибок выше — каждая отброшенная запись объясняет причину.'),
			('Данные неверные или сдвинутые', 'Путаница порядка байт у клиента: регистры MODBUS — '
				'16-битные слова big-endian; проверьте, как клиент собирает 32/64-битные значения.'),
		 ], [52*mm, 111*mm], a_mono_cols=(0,)),
		 H2('7.2.  Справочник кодов сообщений'),
		 tbl(['Код', 'Сер.', 'Когда появляется'], [
			('REVISNF', 'I', 'При старте: версия программы. Указывайте её, когда просите помощи.'),
			('DEVREADY', 'S', 'Последовательный порт открыт; показаны фактическая скорость, '
				'таймаут ответа и интервал t3.5.'),
			('LSNRRDY', 'S', 'TCP-порт слушает; показаны адрес, порт и целевое устройство.'),
			('NETCONN', 'S', 'Подключился TCP-клиент; показаны его адрес:порт и listener.'),
			('NETDISCN', 'S', 'TCP-клиент отключился; показаны его адрес:порт.'),
			('DEVOPNERR', 'E', 'Последовательный порт не открывается; errno объясняет почему '
				'(2 — файла нет, 13 — права, 16 — занят).'),
			('LSNRERR', 'E', 'Отказ bind()/listen() для TCP-порта; errno объясняет почему '
				'(98 — занят, 13 — привилегии).'),
			('NOANSWER', 'E', 'Устройство не ответило за таймаут iotmo.'),
			('FRAMETMO', 'W', 'Ответ начался, но не был завершён за таймаут.'),
			('BADFRAME', 'E', 'Длина принятого кадра вне разумного диапазона [5..256] октетов.'),
			('CRC16ERR', 'E', 'Контрольная сумма принятого кадра не сходится; показаны оба значения.'),
			('EXCRPT', 'W', 'TCP-клиенту возвращается MODBUS-исключение; показаны код и его имя.'),
			('EXITST', 'I', 'Шлюз завершается; показаны флаг выхода и итоговый статус.'),
		 ], [26*mm, 11*mm, 126*mm], a_mono_cols=(0,)),
		 H2('7.3.  Если ничего не помогло'),
		 P('Соберите и приложите к вопросу: 1) полный журнал запуска с /trace (от REVISNF до '
		   'первой ошибки); 2) ваш файл настроек; 3) вывод <font face="Mono">ls -l '
		   '&lt;устройство&gt;</font> и <font face="Mono">dmesg | tail -20</font>; 4) точную '
		   'модель RTU-устройства и его паспортные параметры линии. С этими четырьмя вещами '
		   'проблема почти всегда видна с первого взгляда.')],
		]
	L['chapters'] = chapters
	return L


if __name__ == '__main__':
	l_lang, l_out = sys.argv[1], sys.argv[2]
	build (s_pack_en() if l_lang == 'EN' else s_pack_ru(), l_out)
	print ('%s -> %s' % (l_lang, l_out))
