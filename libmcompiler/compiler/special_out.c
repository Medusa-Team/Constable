/* special out
 * by Marek Zelem
 * copyright (c)1999
 * $Id: special_out.c,v 1.3 2003/12/30 18:01:40 marek Exp $
 */

#include <stdlib.h>
#include <mcompiler/compiler.h>
#include <mcompiler/dynamic.h>

#define	first_data(cd)	((struct to_out_s *)(dfifo_entry((cd),0)))

static void apply_labels( compiler_class_t *c )
{ struct to_out_s *p;
    unsigned long data;
    int i,n;
    n=dfifo_n(c->data);
    for(i=0;i<n;i++)
    {	p=(struct to_out_s *) (dfifo_entry(c->data,i));
        if( p->valid )	continue;
        switch(p->u.rel->typ)
        {	default:
        case 1:	/* data */
            data=p->u.rel->data;
            p->u.rel->used_count--;
            p->valid=1;
            p->u.data=data;
            break;
        case 2:	/* navestie */
            data=p->u.rel->data;
            data-= (c->out_pos)-(dfifo_n(c->data))+i+1;
            p->u.rel->used_count--;
            p->valid=1;
            p->u.data=data;
            c->inf_out(c,OUT_VAL,
                       (c->out_pos)-(dfifo_n(c->data))+i);
            break;
        case 0:	/* undefined */
            break;
        }
    }
}

static void data_flush( compiler_class_t *c )
{ struct to_out_s *p;
    apply_labels(c);
    while( dfifo_n(c->data)>0 )
    {	if( (p=first_data(c->data))->valid )
        {	c->out->out(c->out,p->sym,p->u.data);
            dfifo_read(c->data,NULL,1);
        }
        else	break;
    }
}

static void data_out( compiler_class_t *c, sym_t sym, uintptr_t data )
{ struct to_out_s to;
    data_flush(c);
    if( dfifo_n(c->data)>0 )
    {	to.sym=sym;
        to.valid=1;
        to.u.data=data;
        dfifo_write(c->data,&to,1);
    }
    else	c->out->out(c->out,sym,data);
    c->out_pos++;
}

static void data_rel_out( compiler_class_t *c, sym_t sym, struct label_s *rel )
{ struct to_out_s to;
    to.sym=sym;
    to.valid=0;
    to.u.rel=rel;
    rel->used_count++;
    dfifo_write(c->data,&to,1);
    c->out_pos++;
    data_flush(c);
}

static void sect_start( compiler_class_t *c, unsigned char typ )
{ struct sect_s s;
    s.typ=typ;
    s.labels=NULL;
    s.param_out_data=0;
    s.param_out_ptr=NULL;
    dfifo_write(c->sections,&s,1);
}
#define	SEKCIA(c)	((struct sect_s*)(dfifo_entry((c)->sections,	\
    dfifo_n((c)->sections)-1)))
static void sect_end( compiler_class_t *c )
{ struct label_s *l,*nl;
    if( dfifo_n(c->sections) <= 0 )
    {	c->exit=c->err->error(c->err,END,SEC_END);
        return;
    }
    c->param_out(c,SEC_END);
Next:	apply_labels(c);
    l=SEKCIA(c)->labels;
    while(l!=NULL)
    {	nl=l->next;
        if( l->typ==0 /* undef */ )
        {
            //			if( l->label==0 /*prvy nasledujuci je na konci sekcie*/)
            //			{	l->data=c->out_pos;
            //				l->typ=2; /* navestie */
            //				SEKCIA(c)->labels=l;
            //				goto Next;
            //			}
            c->exit=c->err->error(c->err,SEC_END,l->label);
            l->data=0;
            l->typ=1; /* data */
            SEKCIA(c)->labels=l;
            goto Next;
        }
        free(l);
        l=nl;
    }
    dlifo_read(c->sections,NULL,1);
}
static struct sect_s *get_section( compiler_class_t *c, unsigned char typ )
{ struct sect_s *s;
    int i;
    for(i=dfifo_n(c->sections)-1;i>=0;i--)
    {	s=(struct sect_s*)(dfifo_entry((c)->sections,i));
        if( s->typ==typ )	return(s);
    }
    return(NULL);
}
static struct label_s *get_lab( struct sect_s *sect, unsigned char lab )
{ struct label_s *l;
    for(l=sect->labels;l!=NULL;l=l->next)
    {	if( l->label==lab )
            return(l);
    }
    if( (l=malloc(sizeof(struct label_s)))==NULL )	return(NULL);
    l->label=lab;
    l->used_count=0;
    l->typ=0;	/* undef */
    l->data=0;
    l->next=sect->labels;
    sect->labels=l;
    return(l);
}
static void sect_setpos( compiler_class_t *c, unsigned char lab, int forw )
{ struct label_s *l;
    if( (l=get_lab(SEKCIA(c),lab))==NULL )
    {	c->exit=E|0x0fff;	return;	}
    if( l->typ!=0 /* not undef */ )
        apply_labels(c);
    l->typ=2; /* navestie */
    l->data=c->out_pos;
    c->inf_out(c,SEC_SETPOS,c->out_pos);
    if( forw /* prvy nasledujuci */ )
    {	apply_labels(c);
        l->typ=0; /* undef */
    }
}
static void sect_setdata( compiler_class_t *c, unsigned char lab, unsigned long data, int forw )
{ struct label_s *l;
    if( (l=get_lab(SEKCIA(c),lab))==NULL )
    {	c->exit=E|0x0fff;	return;	}
    if( l->typ!=0 /* not undef */ )
        apply_labels(c);
    l->typ=1; /* data */
    l->data=data;
    if( forw /* prvy nasledujuci */ )
    {	apply_labels(c);
        l->typ=0; /* undef */
    }
}
static void sect_incval( compiler_class_t *c, unsigned char lab, int a )
{ struct label_s *l;
    if( (l=get_lab(SEKCIA(c),lab))==NULL )
    {	c->exit=E|0x0fff;	return;	}
    if( l->typ==0 )
    {	l->typ=1; l->data=0;	}
    if( l->typ!=1 )
    {	c->exit=c->err->error(c->err,SEC_INCVAL,END);
        return;
    }
    l->data+=a;
}
static void sect_getval( compiler_class_t *c, unsigned char typ, unsigned char lab )
{ struct sect_s *s;
    struct label_s *l;
    if( (s=get_section(c,typ))==NULL )
    {	c->exit=c->err->error(c->err,SEC_START|typ,END);
        return;
    }
    if( (l=get_lab(s,lab))==NULL )
    {	c->exit=E|0x0fff;	return;	}
    if( c->l_rel!=NULL )
        c->l_rel->used_count--;
    c->l_rel= l;
    l->used_count++;
    c->l.data=0;	/* ??? */
}
static void sect_outval( compiler_class_t *c, unsigned char typ, unsigned char lab )
{ struct sect_s *s;
    struct label_s *l;
    if( (s=get_section(c,typ))==NULL )
    {	c->exit=c->err->error(c->err,SEC_START|typ,END);
        return;
    }
    if( (l=get_lab(s,lab))==NULL )
    {	c->exit=E|0x0fff;	return;	}
    data_rel_out(c,OUT_VAL,l);
}

void compiler_buildin1( compiler_class_t *compiler, sym_t sym )
{
    if( (sym & TYP)==O )
    {	if( compiler->l_rel==NULL )
            data_out(compiler,sym,compiler->l.data);
        else	data_rel_out(compiler,sym,compiler->l_rel);
    }
    else if( (sym & TYP)==VAL )
    {	compiler->l.data= sym & SYM_VAL;
        if( compiler->l_rel!=NULL )
        {	compiler->l_rel->used_count--;
            compiler->l_rel= NULL;
        }
    }
    else if( (sym & TYP)==SO1 )
    {	if( (sym&(~0x0f))==SEC_START )
            sect_start(compiler,sym & 0x0f);
        else if( sym==SEC_END )
            sect_end(compiler);
        else if( sym==OUT_LEX )
            data_out(compiler,sym,(unsigned long)(compiler->l.sym));
        else if( sym==OUT_VAL )
        {	if( compiler->l_rel==NULL )
                data_out(compiler,sym,compiler->l.data);
            else	data_rel_out(compiler,sym,compiler->l_rel);
        }
        else if( (sym&(~0x0ff))==SEC_SETPOS )
            sect_setpos(compiler,sym & 0x0ff,0);
        else if( (sym&(~0x0ff))==SEC_SETLEX )
            sect_setdata(compiler,sym & 0x0ff,
                         (unsigned long)(compiler->l.sym),0);
        else if( (sym&(~0x0ff))==SEC_SETVAL )	/* !!! poroz na rel */
            sect_setdata(compiler,sym & 0x0ff,compiler->l.data,0);
        else if( (sym&(~0x0ff))==SEC_SEFPOS )
            sect_setpos(compiler,sym & 0x0ff,1);
        else if( (sym&(~0x0ff))==SEC_SEFLEX )
            sect_setdata(compiler,sym & 0x0ff,
                         (unsigned long)(compiler->l.sym),1);
        else if( (sym&(~0x0ff))==SEC_SEFVAL )	/* !!! poroz na rel */
            sect_setdata(compiler,sym & 0x0ff,compiler->l.data,1);
        else if( (sym&(~0x0ff))==SEC_INCVAL )
            sect_incval(compiler,sym & 0x0ff,1);
        else if( (sym&(~0x0ff))==SEC_DECVAL )
            sect_incval(compiler,sym & 0x0ff,-1);
    }
    else if( (sym & TYP)==SO2 )	/* OUT_VALx */
    {	sect_outval(compiler,(sym>>8)&0x0f,sym & 0x0ff);
    }
    else if( (sym & TYP)==SO3 )	/* GET_VALx */
    {	sect_getval(compiler,(sym>>8)&0x0f,sym & 0x0ff);
    }
    else if( sym==TEND )	/* koniec prekladu */
    {	while( dfifo_n(compiler->sections) > 0 )
            sect_end(compiler);
        data_flush(compiler);
    }
    else
    {	if( compiler->l_rel==NULL )
            data_out(compiler,sym,compiler->l.data);
        else	data_rel_out(compiler,sym,compiler->l_rel);
    }
}

