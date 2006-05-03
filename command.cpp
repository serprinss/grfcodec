/*
 * command.cpp
 * Defines functions for comment commands.
 *
 * Copyright 2004-2006 by Dale McCoy.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include<iostream>
#include<iomanip>
#include<sstream>
#include<string>
#include<cassert>
#include<stack>
#ifdef _MSC_VER
#   pragma warning(disable:4702)//unreachable code
#   include<map>
#   pragma warning(default:4702)
#else
#   include<map>
#endif
using namespace std;

#include"globals.h"
#include"renum.h"
#include"command.h"
#include"inlines.h"
#include"sanity.h"
#include"messages.h"
#include"inject.h"
#include"sanity_defines.h"

#include"ExpandingArray.h"

#define CASE(_case,_return)\
	case _case:return _commandState._return;

struct commandData{
	char*name;
	int value;
};

#undef _RENUM_COMMAND_H_INCLUDED_
#undef COMMAND_DATA_START
#undef COMMAND_DATA
#undef COMMAND_DATA_EX
#undef COMMAND_DATA_END
#define COMMAND_DATA_START(x) const commandData x[]={
#define COMMAND_DATA(x) {#x,x},
#define COMMAND_DATA_EX(val,name) {#name,val},
#define COMMAND_DATA_END() {NULL,-1}};

#include "command.h"

struct command{
	command();
	uint sanity_messages,extensions;//,verbose;
	int real:2;
	bool remove_messages,beautifier,diff,locked;
	uint beauty;
	Expanding0Array<int>warnstate;
}_commandState,_CLstate;

static map<string,int>_varmap,_CLvar;

const command&crCommandState=_commandState;

command::command(){
	remove_messages=true;
	beautifier=diff=locked=false;
	sanity_messages=WARNING3;
	real=0;
	beauty=686031766;
	extensions=0;
//	verbose=0;
}

int find_command(const string&command,const commandData type[]){
	for(int i=0;;i++){
		if(type[i].name==NULL)return -1;
		if(UCase(command)==type[i].name)return type[i].value;
	}
}

bool is_command(const string&line){
	assert(is_comment(line));
	size_t x=line.find_first_not_of(COMMENT+WHITESPACE);
	return line[x]=='@'&&line[x+1]=='@';
}

bool is_message(const string&line){
	assert(is_comment(line));
	size_t x=line.find_first_not_of(COMMENT+WHITESPACE);
	return line[x]=='!'&&line[x+1]=='!';
}

void reset_commands(){
	_commandState=_CLstate;
	_varmap=_CLvar;
}

string GetOnOffString(const string&str){
	if(str=="+")return "ON";
	if(str=="-")return "OFF";
	return str;
}

void CLCommand(int command){
	bool locked=_commandState.locked;
	_commandState.locked=false;
	switch(command){
	case'd':parse_comment("//@@DIFF");break;
	case'L':parse_comment("//@@LET "+string(optarg));break;
	case'l':parse_comment("//@@LINT "+GetOnOffString(optarg));break;
	case'r':parse_comment("//@@REALSPRITES "+string(optarg));break;
	case'b':parse_comment("//@@BEAUTIFY "+GetOnOffString(optarg));break;
	case'p':_commandState.remove_messages=false;break;
	case'e':parse_comment("//@@EXTENTIONS "+GetOnOffString(optarg));break;
	case 256:locked=true;break;
	case'w':case'W':{
		istringstream arg(optarg);
		int opt;
		while(arg>>opt){
			parse_comment((command=='w'?"//@@WARNING DISABLE ":"//@@WARNING ENABLE ")+itoa(opt));
			arg.ignore();
		}
		break;
	}DEFAULT(command)
	}
	_commandState.locked=locked;
	_CLstate=_commandState;
	_CLvar=_varmap;
}

void SetVar(const string&,const string&);
string ReadVar(istream&);

bool parse_comment(const string&line){
	assert(is_comment(line));
	if(is_message(line))
		return!GetState(REMOVEMESSAGES);
	if(!is_command(line))
		return true;
	string command=line.c_str()+line.find_first_not_of(COMMENT+WHITESPACE+'@'),command_part;
	while(command.find('=')!=NPOS)command[command.find('=')]=' ';
	istringstream commandstream(command);
	commandstream>>command_part;
	int id;
	switch(find_command(command_part,gen)){
	case REMOVEMESSAGES:
		_commandState.remove_messages=true;
		break;
	case PRESERVEMESSAGES:
		_commandState.remove_messages=false;
		break;
	case LINT:
		if(commandstream>>command_part){
			if(GetState(LINT)!=OFF){
				int x=find_command(command_part,san);
				_commandState.sanity_messages=(x==-1?WARNING3:x);
			}
			break;
		}
		return true;
	case USEID2:{
		int feature;
		commandstream>>setbase(16)>>feature>>id;
		if(!IsValidFeature(ACT2,feature)){
			IssueMessage(0,COMMAND_INVALID_ARG,gen[USEID2].name);
			return true;
		}
		sanity_use_id(feature,id);
		return true;
	}case USESET:
		commandstream>>setbase(16)>>id;
		sanity_use_set(id);
		return true;
	case DIFF:
		_commandState.diff=_commandState.remove_messages=true;
		_commandState.sanity_messages=OFF;
		if(_commandState.locked)_commandState=_CLstate;
		return false;
	case WARNING:{
		commandstream>>command_part;
		int num=0,state=find_command(command_part,warn);
		commandstream>>num;
		if(state==-1){
			IssueMessage(0,COMMAND_UNKNOWN,command_part.c_str());
			IssueMessage(0,COMMAND_REVERT_DEFAULT);
		}
		_commandState.warnstate[num]=state;
		break;
	}case VERSIONCHECK:{
		commandstream>>command_part;
		int ver;
		if(command_part.length()==8&&command_part.find_first_not_of(VALID_PSEUDO)==NPOS)
			ver=ctoi(command_part[0])<<28|ctoi(command_part[1])<<24|ctoi(command_part[2])<<20|ctoi(command_part[3])<<16|
				ctoi(command_part[4])<<12|ctoi(command_part[5])<<8|ctoi(command_part[6])<<4|ctoi(command_part[7]);
		else{
			uint M,m,r,b;
			if(sscanf(command_part.c_str(),"%u.%u.%u.%u",&M,&m,&r,&b)==4&&M<256&&m<16&&r<16&&b<65536)
				ver=M<<24|m<<20|r<<16|b;
			else if((sscanf(command_part.c_str(),"201a%u",&b)==1||sscanf(command_part.c_str(),"2.0.1a%u",&b)==1)&&b<6553)
				ver=0x020A0000|(b*10);
			else if((sscanf(command_part.c_str(),"25b%u",&b)==1||sscanf(command_part.c_str(),"2.5b%u",&b)==1)&&b<6553)
				ver=0x02500000|(b*10);
			else{
				IssueMessage(0,COMMAND_INVALID_ARG,gen[VERSIONCHECK].name);
				return true;
			}
		}
		getline(commandstream,command_part);
		inject(mysprintf("0*0 09 8B 04 05 %8x 01",ver-1));
		inject(mysprintf("0*0 0B 03 1F 00%s 00",command_part.c_str()));
		//inject("//@@PRESERVEMESSAGES NOPRESERVE");
		inject(mysprintf("0*0 09 8B 04 04 %8x 00"/* %s!! MOVE ME AFTER THE ACTION 8 !!"*/,ver,COMMENT_PREFIX));
		//if(GetState(REMOVEMESSAGES))inject("//@@REMOVEMESSAGES NOPRESERVE");
		return false;
	}case LET:{
		string var=ReadVar(commandstream);
		if(var=="")return true;
		if(eat_white(commandstream).peek()=='=')commandstream.ignore();
		getline(eat_white(commandstream),command_part);
		if(!_commandState.locked)SetVar(var,command_part);
		return true;
	}case REALSPRITES:
		commandstream>>command_part;
		switch(find_command(command_part,real)){
		case RPNON:_commandState.real&=~1;break;
		case RPNOFF:_commandState.real|=3;break;//Yes, 3. RPNOFF also sets COMMENTOFF.
		case COMMENTON:_commandState.real&=~2;break;
		case COMMENTOFF:_commandState.real|=2;break;
		case-1:IssueMessage(0,COMMAND_INVALID_ARG,gen[REAL].name);return true;
		default:
			INTERNAL_ERROR(command,find_command(command_part,real));
		}
		break;
	case BEAUTIFY:{
		commandstream>>command_part;
		uint val=find_command(command_part,beaut);
		if(val!=(uint)-1&&val!=OFF)_commandState.beautifier=true;
		switch(val){
		case -1:
			IssueMessage(0,COMMAND_INVALID_ARG,gen[BEAUTIFY].name);
			return true;
		case OFF:_commandState.beautifier=false;break;
		case ON:break;
		case MAXLEN:
			if(!(commandstream>>val)||val>255){
				IssueMessage(0,COMMAND_INVALID_ARG,gen[BEAUTIFY].name);
				commandstream.ignore(INT_MAX);
				return true;
			}
			_commandState.beauty=_commandState.beauty&~0xFF|val;
			break;
		case QUOTEHIGHASCII:
			commandstream>>command_part;
			val=find_command(command_part,beaut);
			if(!commandstream||val==(uint)-1){
				IssueMessage(0,COMMAND_INVALID_ARG,gen[BEAUTIFY].name);
				return true;
			}
			if(val==OFF)_commandState.beauty&=~0x100;
			else _commandState.beauty|=0x100;
			break;
		case QUOTEUTF8:
			commandstream>>command_part;
			val=find_command(command_part,beaut);
			if(!commandstream||val==(uint)-1){
				IssueMessage(0,COMMAND_INVALID_ARG,gen[BEAUTIFY].name);
				return true;
			}
			if(val==OFF)_commandState.beauty&=~0x200;
			else _commandState.beauty|=0x200;
			break;
		case HEXGRFID:
			commandstream>>command_part;
			val=find_command(command_part,beaut);
			if(!commandstream||val==(uint)-1){
				IssueMessage(0,COMMAND_INVALID_ARG,gen[BEAUTIFY].name);
				return true;
			}
			if(val==ON)_commandState.beauty|=0x400;
			else _commandState.beauty&=~0x400;
			break;
		case LEADINGSPACE:{
			commandstream>>command_part;
			uint lead[3]={0,0,0};
			int count=sscanf(command_part.c_str(),"%u,%u,%u",lead,lead+1,lead+2);
			if(lead[0]==0)lead[0]=GetState(LEADINGSPACE);
			if(lead[1]==0)lead[1]=GetState(LEADINGSPACE,1);
			if(lead[2]==0)lead[2]=GetState(LEADINGSPACE,2);
			while(count&&count!=3){
				lead[count]=lead[count-1]+3;
				count++;
			}
			if(lead[0]>32||lead[1]>32||lead[2]>32){
				IssueMessage(0,COMMAND_INVALID_ARG,gen[BEAUTIFY].name);
				return true;
			}
			_commandState.beauty=_commandState.beauty&~0x7FFF0000|((lead[0]-1)<<16)|((lead[1]-1)<<21)|((lead[2]-1)<<26);
			break;
		}case LINEBREAKS:{
			uint breaks=4;
			commandstream>>breaks;
			if(breaks>3){
				IssueMessage(0,COMMAND_INVALID_ARG,gen[BEAUTIFY].name);
				return true;
			}
			_commandState.beauty&=~0x1800;
			_commandState.beauty|=((breaks+2)&3)<<11;
			break;
		}case CONVERTONLY:{
			commandstream>>command_part;
			val=find_command(command_part,beaut);
			if(!commandstream||val==(uint)-1){
				IssueMessage(0,COMMAND_INVALID_ARG,gen[BEAUTIFY].name);
				return true;
			}
			if(val==ON)_commandState.beauty|=0x80000000;
			else _commandState.beauty&=~0x80000000;
			break;
		}case GETCOOKIE:
			inject(mysprintf("%s@@BEAUTIFY SETCOOKIE %d",COMMENT_PREFIX,_commandState.beauty));
			return false;
		case SETCOOKIE:
			if(!(commandstream>>_commandState.beauty)){
				IssueMessage(0,COMMAND_INVALID_ARG,gen[BEAUTIFY].name);
				return true;
			}
			break;
		default:
			INTERNAL_ERROR(command,find_command(command_part,beaut));
		}
		commandstream>>command_part;
		if(_commandState.locked)_commandState=_CLstate;
		return find_command(command_part,preserve)!=NOPRESERVE;
	}case CLEARACTION2:
		final123();
		Init123();
		AutoConsoleMessages();//final123 calls ManualConsoleMessages();
		break;
	case CLEARACTIONF:
		finalF();
		InitF();
		AutoConsoleMessages();//finalF calls ManualConsoleMessages();
		break;
	case EXTENSIONS:
		commandstream>>command_part;
		if(!commandstream)return true;
		id=find_command(command_part,ext);
		if(id==ON)_commandState.extensions=1;
		else if(id==OFF)_commandState.extensions=0;
		else{
			IssueMessage(0,COMMAND_INVALID_ARG,gen[EXTENSIONS].name);
			return true;
		}
		break;
	/*case VERBOSE:{
		uint level;
		if(!(commandstream>>level)||level>2){
			IssueMessage(0,COMMAND_INVALID_ARG,gen[VERBOSE].name);
			return true;
		}
		_commandState.verbose=level;
		break;
	}*/
/* Add case condition for new comment commands here.
 * The order in this function should be the same as the order in command.h.
 */
	case -1:
		IssueMessage(0,COMMAND_UNKNOWN,command_part.c_str());
		return true;
	default:
		INTERNAL_ERROR(find_command(command_part,gen),find_command(command_part,gen));
	}
	commandstream>>command_part;
	if(_commandState.locked)_commandState=_CLstate;
	return find_command(command_part,preserve)!=NOPRESERVE;
}

int GetState(enum gen type){
	switch(type){
		CASE(REMOVEMESSAGES,remove_messages);
		CASE(LINT,sanity_messages);
		CASE(DIFF,diff);
		CASE(REALSPRITES,real);
		CASE(BEAUTIFY,beautifier);
		CASE(EXTENSIONS,extensions);
//		CASE(VERBOSE,verbose);
/* Add CASE(COMMAND,variable) macros for new comment commands here.
 * The order in this function should be the same as the order in command.h.*/
		DEFAULT(type);
	}
}


uint GetState(enum beaut type,int arg){
	const uint&beaut=_commandState.beauty;
	switch(type){
		case MAXLEN:return beaut&0xFF;
		case LEADINGSPACE:
			return(beaut>>(16+5*arg))&0x1F;
		case QUOTEHIGHASCII:return beaut&0x100;
		case QUOTEUTF8:return beaut&0x200;
		case HEXGRFID:return beaut&0x400;
		case LINEBREAKS:return(((beaut&0x1800)>>11)+2)&3;
		case CONVERTONLY:return beaut&0x80000000;
		DEFAULT(type);
	}
}

#undef MESSAGE
#undef START_MESSAGES
#undef END_MESSAGES
#undef MESSAGE_EX

#define MESSAGE(name,message,props)name,
#define START_MESSAGES() int msg_type[]={
#define END_MESSAGES() };
#define MESSAGE_EX(name,msg,props,loc,base) base,

#undef _RENUM_MESSAGES_H_INCLUDED_

#include"messages.h"

bool GetWarn(int message,int minSan){
	message=msg_type[message];
	switch(crCommandState.warnstate[message]){
	case ENABLE:return true;
	case DISABLE:if(minSan>=ERROR)return false;
	}
	return GetState(LINT)>=minSan;
}

void SetVar(const string&var,const string&value){
	int val;
	string::size_type offs;
	if((offs=value.find('('))==NPOS){
		const char*pch=value.c_str();
		while(isspace(*pch))pch++;
		val=atoi(pch);
	}else{
		val=DoCalc(value,offs);
		if(offs==NPOS)return;
	}
	_varmap.insert(map<string,int>::value_type(var,val));
}

int GetVar(const string&var,int&err){
	map<string,int>::const_iterator it;
	if((it=_varmap.find(var))!=_varmap.end())return it->second;
	IssueMessage(0,err=UNDEF_VAR,var.c_str());
	SetCode(EPARSE);
	return 0;
}

string ReadVar(istream&in){
	eat_white(in);
	int ch;
	string delim="=()+/-*",var;
	while(!isspace(ch=in.get())&&delim.find((char)ch)==NPOS&&ch!=EOF)var+=(char)ch;
	if(ch==EOF)var="";
	else in.unget();
	return var;
}

int DoCalc(istream&data,int&err){
	stack<int>nums;
	string ops="+-/*";
	int ch,l;
	while(true){
		ch=eat_white(data).peek();
		if(isdigit(ch)){
			data>>l;
			nums.push(l);
		}else if(ops.find((char)ch)!=NPOS){
			data.ignore();
			if(nums.size()<2){
				IssueMessage(0,err=BAD_RPN,ch);
				return 0;
			}
			int r;
			r=nums.top();
			nums.pop();
			l=nums.top();
			switch(ch){
			case'+':nums.top()=l+r;break;
			case'-':nums.top()=l-r;break;
			case'*':nums.top()=l*r;break;
			case'/':nums.top()=l/r;break;
			DEFAULT(ch);
			}
		}else if(ch==')'){
			if(!nums.size()){
				IssueMessage(0,err=BAD_RPN,ch);
				return 0;
			}
			return nums.top();
		}else if(ch==EOF){
			IssueMessage(0,err=BAD_RPN_EOF);
			return 0;
		}else{
			string var=ReadVar(data);
			nums.push(GetVar(var,err));
			if(err)return 0;
		}
	}
}

int DoCalc(const string&data,string::size_type&offs){
	istringstream in(data);
	in.ignore((int)offs+1);
	int err=0,ret=DoCalc(in,err);
	offs=err?NPOS:data.find(')',offs)+1;
	return ret;
}

/*int DoCalc(istringstream&data,int&err){
	char ch=eat_white(data).peek();
	int l,r,op;
	if(isdigit(ch))data>>l;
	else if(ch=='('){
		l=DoCalc(data.ignore(),err);
		if(err)return l;
	}else{
		string var;
		data>>var;
		l=GetVar(var,err);
		if(err)return l;
	}
	eat_white(data).get(ch);
	if(ch==')')return l;
	string ops="+-*" "/";
	int op=(int)ops.find(ch);
	if(op==NPOS){
		err=NOT_OP;
		return ch;
	}
	if(isdigit(ch))data>>r;
	else if(ch=='('){
		r=DoCalc(data.ignore(),err);
		if(err)return r;
	}else{
		string var;
		data>>var;
		r=GetVar(var,err);
		if(err)return r;
	}
	switch(op){
	case 0:return l+r;
	case 1:return l-r;
	case 2:return l*r;
	case 3:return l/r;
	DEFAULT(DoCalc,op);
	}
}
int DoCalc(const string&data,size_t&offs){
	char ch;
	int l=0,r=0,op=0;
	while(true){
		if(isspace(ch=data[offs++]))continue;
		if(ch=='(')(op?r:l)=DoCalc(data,offs);
		else if(ops.find(ch)!=NPOS)op=ops.find(ch)+1;
		else if(ch>='0'&&ch<='9'){
			(op?r:l)*=10;
			(op?r:l)+=ch-'0';
		}else if(ch==')')

}*/