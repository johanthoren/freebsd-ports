# $FreeBSD$

PORTNAME=	imhex
PORTVERSION=	1.7.0
PORTREVISION=	1
CATEGORIES=	editors

MAINTAINER=	nobutaka@FreeBSD.org
COMMENT=	Hex editor for reverse engineers and programmers

LICENSE=	GPLv2
LICENSE_FILE=	${WRKSRC}/LICENSE

NOT_FOR_ARCHS=	i386
NOT_FOR_ARCHS_REASON=	__uint128_t and __int128_t are not supported

BUILD_DEPENDS=	c++10:lang/gcc10 \
		glm>0:math/glm \
		nlohmann-json>0:devel/nlohmann-json
LIB_DEPENDS=	libcapstone.so:devel/capstone4 \
		libmbedtls.so:security/mbedtls \
		libfreetype.so:print/freetype2 \
		libglfw.so:graphics/glfw \
		libstdc++.so:lang/gcc10 \
		libtre.so:textproc/libtre

USES=		cmake pkgconfig python:3.8+ xorg
USE_XORG=	x11 xcb xau xdmcp
USE_GITHUB=	yes
GH_ACCOUNT=	WerWolv
GH_PROJECT=	ImHex
GH_TAGNAME=	v${PORTVERSION}

CC=	${LOCALBASE}/bin/gcc10
CXX=	${LOCALBASE}/bin/c++10

PORTDOCS=	README.md

OPTIONS_DEFINE=	DOCS NLS
NLS_USES=	gettext

post-patch:
	${REINPLACE_CMD} -e "s|%%PREFIX%%|${PREFIX}|" ${WRKSRC}/source/window.cpp

post-install:
	${STRIP_CMD} ${STAGEDIR}${PREFIX}/share/imhex/plugins/builtin.hexplug
	${STRIP_CMD} ${STAGEDIR}${PREFIX}/share/imhex/plugins/example.hexplug

post-install-DOCS-on:
	${MKDIR} ${STAGEDIR}${DOCSDIR}
	${INSTALL_DATA} ${WRKSRC}/README.md ${STAGEDIR}${DOCSDIR}

.include <bsd.port.mk>
