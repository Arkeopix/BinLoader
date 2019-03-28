SRC=		main.c \
			loader.c

OBJ=		${SRC:.c=.o}

NAME=		binloader

CFLAGS+=	-W -Wall -Werror \
			-ansi -pedantic -Wpointer-arith \
			--std=c11 -g

LFLAGS+=	-lbfd

CC=			gcc

${NAME}:   	${OBJ}
			${CC} -o ${NAME} ${OBJ} ${CFLAGS} ${LFLAGS}

all:		${NAME}

clean:
			find . \( -name "*~" -o -name "*.o" -o -name "#*#" \) -exec rm -rf {} \;

fclean:		clean
			rm -rf ${NAME}

re:			fclean all
