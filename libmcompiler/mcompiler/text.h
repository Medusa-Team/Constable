/* text library   */
/* (c) by Marek Zelem */
/* $Id: text.h,v 1.3 2002/10/23 10:25:44 marek Exp $	*/

#ifndef	_TEXT_H
#define	_TEXT_H

#include <mcompiler/dynamic.h>

int wildcmp(char *str,char *wild);

/* format:  <> - poznamka
 *          "" - retazec, ktory musi obsahovat
 *          {"",""} - jeden z tychto retazcov
 *          %w - vstupne slovo
 *          %d - vstupne cislo
 *          %[] - vstupne slovo z tychto znakov
 *          %{} - vstupne cislo - poradove cislo retazca
 *          %"" - vstupny retazec - ktory sadne
 *          %D - vstupne cislo dna v tyzdni (Su=0 .. Sa=6)
 *          %M - vstupne cislo mesiaca (Jan=0 .. Dec=11)
 *          [] - nepovinne
 *          [: :] - opakovane
 *            - (medzera) oddelovac argumentov
 *          $ - koniec argumentov
 * argv: vstupne argumenty
 * sout a iout - vystupne - vytvori ich arg_parse
 */
int arg_parse( char *format, char *argv[], darg_t **sout, ddata_t **iout );
int arg_parse_free( darg_t **sout, ddata_t **iout );



#endif /* _TEXT_H */

