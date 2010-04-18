CPPFLAGS= -DMONOPD_DATADIR=\"$(pkgdatadir)\" -DMONOPD_CONFIGDIR=\"$(sysconfdir)\"

bin_PROGRAMS = monopd

monopd_SOURCES = auction.cpp card.cpp cardgroup.cpp debt.cpp display.cpp \
	estate.cpp estategroup.cpp event.cpp game.cpp gameconfig.cpp \
	gameobject.cpp gameproperty.cpp io.cpp main.cpp player.cpp \
	server.cpp trade.cpp tradecomponent.cpp

noinst_HEADERS = auction.h card.h cardgroup.h debt.h display.h estate.h \
	estategroup.h event.h game.h gameconfig.h gameobject.h \
	gameproperty.h io.h main.h player.h server.h trade.h \
	tradecomponent.h const.h

monopd_LDADD = -lcapsinetwork -lmath++
