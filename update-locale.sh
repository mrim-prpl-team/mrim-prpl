#!/bin/sh
xgettext -k_ -kN_ --from-code=utf-8 -o po/mrim-prpl-underbush.pot *.c *.h
#mv po/mrim-prpl-underbush-ru_RU.po po/mrim-prpl-underbush-ru_RU.po.old
#msgmerge po/mrim-prpl-underbush.pot po/mrim-prpl-underbush-ru_RU.po.old > po/mrim-prpl-underbush.pot
#rm po/mrim-prpl-underbush-ru_RU.po po/mrim-prpl-underbush-ru_RU.po.old
msgmerge -U po/mrim-prpl-underbush-ru_RU.po po/mrim-prpl-underbush.pot
