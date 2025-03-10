#include "compiler.h"

void compiler::initialize()
{
	GenerateStandardArea();
}

//area can contain external functions and variables that you want to be able to call in the code
area* compiler::Compile(std::wstring code, area* a)
{
	//split in functions
	//first, remove all comments
	std::vector<std::wstring> lines = GetCodeLines(code);
	area* localarea = Compile(lines, a);
	return localarea;

}

area* compiler::Compile(std::vector<std::wstring> lines, area* parent)
{
	area* cur = new area();
	cur->Parent = parent;
	for (int i = 0; i < lines.size(); i++)
	{
		std::wstring line = lines[i];
		if (line.length() > 2) //smallest line of code: x++
		{
			wstringcontainer words = split_string(line, L" ");
			int space1index = line.find(' ');
			int space2index = line.find(' ', space1index + 1);

			bool isconst = substrcomp(words[0], L"const");//const or constexpr, does not matter
			int typeindex = isconst ? 1 : 0;
			types vartype = GetType(words[typeindex]);
			bool isvar = vartype >= 0;
			if (isvar) //declaration
			{
				if (i + 1 < lines.size() && lines[i + 1] == L"{")//function or array
				{
					//seek braces
					int brace1 = i + 1;
					int brace2 = FindLine(lines, brace1 + 1, L"}", wstringcontainer{ L"{",L"}" });
					if (brace2 < 0)
					{
						ShowCompilerError(noclosingbrace, line);
					}
					//arguments
					int index1 = line.find('(');
					int index2 = Find(line, index1 + 1, L")", skip);
					if (index2 >= 0) {
						//function that returns vartype
						internalfunction* f = new internalfunction();
						f->name = line.substr(space1index + 1, index1 - space1index - 1);
						std::wstring argumentspace = line.substr(index1 + 1, index2 - index1 - 1);
						if (argumentspace.length())
						{
							wstringcontainer arguments = split_string(argumentspace, L",");
							for (int i = 0; i < arguments.size(); i++)
							{
								trim(arguments[i]);
								int spaceindex = arguments[i].find(' ');
								variable* v = new variable(GetType(trim_copy(arguments[i].substr(0, spaceindex))), 
									true,//is modifyable, because the arguments will be copied
									trim_copy(arguments[i].substr(spaceindex + 1, arguments[i].size() - spaceindex - 1)));
								f->arguments.push_back(v);
							}
						}
						area* WithArguments = new area();
						WithArguments->Parent = cur;
						WithArguments->variables = f->arguments;
						//copy lines into a new array
						wstringcontainer functionlines = wstringcontainer{ &lines[brace1 + 1], &lines[brace2] };

						area* a = Compile(functionlines, WithArguments);
						f->actions = a->actions;
						cur->functions.push_back(f);
						delete a;//the area is not necessary anymore
					}
					else //array
					{
						//get name
						int bracket0 = line.find('[') + 1;
						int bracket1 = line.find(']');
						std::wstring namewithbrackets = words[typeindex + 1];
						variable* v = new variable();
						v->name = namewithbrackets.substr(0, namewithbrackets.find('['));
						long count;
						strtol(line.substr(bracket0, bracket1 - bracket0), count, 10);
						v->type = vartype;
						std::wstring values = lines[i + 2];
						wstringcontainer valcontainer = split_string(values, L",");
						if (valcontainer.size() != count) 
						{
							ShowCompilerError(notallindexesinitialized,namewithbrackets);
						}
						switch (vartype)
						{
						case tbool:
							v->var = (void*)new bool[count];
							for (int i = 0; i < count; i++)
							{
								trim(valcontainer[i]);
								((bool*)v->var)[i] = valcontainer[i] == L"true";
							}
							break;
						case tint:
							v->var = (void*)new int[count];
							for (int i = 0; i < count; i++)
							{
								trim(valcontainer[i]);
								long l;
								strtol(valcontainer[i], l, 10);
								((int*)v->var)[i] = (int)l;
							}
							break;
						case tfp:
							v->var = (void*)new fp[count];
							for (int i = 0; i < count; i++)
							{
								trim(valcontainer[i]);
								fp d;
								strtod(valcontainer[i], d);
								((fp*)v->var)[i] = d;
							}
							break;
						}
						cur->variables.push_back(v);
					}
					i = brace2;//warp to end of function
					continue;
				}
				else
				{//variable
					std::wstring namepart = line.substr(isconst ? space2index : space1index);
					wstringcontainer names = split_string(namepart, L",", skip);
					for (int i = 0; i < names.size(); i++)
					{
						trim(names[i]);
						std::wstring declaration = names[i];
						if (declaration.length() > 0) {
							variable* Declared = new variable(vartype, true);
							cur->variables.push_back(Declared);

							int equalsindex = declaration.find('=');
							if (equalsindex > 0)//minimum chars: a=
							{
								//instant initialization
								Declared->name = trim_copy(declaration.substr(0, equalsindex));//will be trimmed later
								calculation* c = cur->GetCalculation(declaration, NULL);
								if (c) {
									cur->actions.push_back(c);
								}
							}
							else
							{
								Declared->name = trim_copy(names[i]);
							}
						}
					}
				}
			}
			else if (substrcomp(line, L"for"))
			{
				//for loop
				loop* l = new loop();

				int index1 = line.find('(') + 1;
				int index2 = Find(line, index1, L")", skip);
				std::wstring betweenparentheses = line.substr(index1, index2 - index1);
				//split line
				wstringcontainer parenthesescontainer = split_string(betweenparentheses, L";");
				//seek brackets
				int bracket1 = i + 1;
				int bracket2 = FindLine(lines, bracket1 + 1, L"}", wstringcontainer{ L"{", L"}" });
				//between the brackets
				wstringcontainer bracketscontainer = wstringcontainer{ &lines[bracket1 + 1],&lines[bracket2] };
				bracketscontainer.push_back(parenthesescontainer[2]);//at the end of each iteration

				//do this once
				area* abegin = Compile(wstringcontainer{ parenthesescontainer[0] }, cur);
				cur->actions.insert(cur->actions.end(), abegin->actions.begin(), abegin->actions.end());
				cur->variables.insert(cur->variables.end(), abegin->variables.begin(), abegin->variables.end());

				area* a = Compile(bracketscontainer, cur);
				l->actions = a->actions;//copy actions
				l->checkresult = nullptr;
				if (action* check = a->GetCalculation(parenthesescontainer[1], &l->checkresult))
				{
					l->actions.push_back(check);//at the end of each iteration and
					cur->actions.push_back(check);//before the iteration starts
				}
				cur->actions.push_back(l);//insert loop
				i = bracket2;//warp to end of statement
				continue;
			}
			else if (substrcomp(line, L"if"))
			{
				//if statement
				ifstatement* s = new ifstatement();

				int index1 = line.find('(') + 1;
				int index2 = Find(line, index1, L")", skip);
				std::wstring betweenparentheses = line.substr(index1, index2 - index1);
				if (action* check = cur->GetCalculation(betweenparentheses, &s->checkresult))
				{
					cur->actions.push_back(check);//before the statement is called, the variables will be checked
				}

				//seek brackets
				int truebracket1 = i + 1;
				int truebracket2 = FindLine(lines, truebracket1 + 1, L"}", wstringcontainer{ L"{", L"}" });
				wstringcontainer bracketscontainer = wstringcontainer{ &lines[truebracket1 + 1],&lines[truebracket2] };

				area* truea = Compile(bracketscontainer, cur);

				s->actionswhentrue = truea->actions;
				//check for else
				if (substrcomp(lines[truebracket2 + 1], L"else"))
				{
					int falsebracket1 = truebracket2 + 2;//skip the 'else' sentence
					int falsebracket2 = FindLine(lines, falsebracket1 + 1, L"}", wstringcontainer{ L"{", L"}" });
					wstringcontainer falsebracketscontainer = wstringcontainer{ &lines[falsebracket1 + 1],&lines[falsebracket2] };

					area* falsea = Compile(falsebracketscontainer, cur);

					s->actionswhenfalse = falsea->actions;
					i = falsebracket2;//warp to end of statement
				}
				else
				{
					i = truebracket2;//warp to end of statement
				}
				cur->actions.push_back(s);//insert statement
				continue;
			}
			else
			{
				calculation* c = cur->GetCalculation(line, NULL);//return variable will not be used
				cur->actions.push_back(c);
			}
		}
		else if (line.length())
		{
			ShowCompilerError(linetooshort, line);
		}
	}
	return cur;
}

std::wstring RemoveComments(std::wstring str) 
{
	int lastindex = 0;
	while (true)
	{
		int newindex = str.find(L"//", lastindex);
		if (newindex == std::wstring::npos)
		{
			return str;
		}
		else
		{
			int newlineindex = str.find(L'\n', newindex + 1);
			str = str.substr(0, newindex) + str.substr(newlineindex + 1);//remove the '\n' too
			lastindex = newindex;//jump just over the begin of the cut
		}
	}
}
std::vector<std::wstring> compiler::GetCodeLines(std::wstring code)
{
	//remove comments
	code = RemoveComments(code);
	code = replace(code, L"\n", L"");
	code = replace(code, L"\r", L"");//sort of \n
	code = replace(code, L"\t", L" ");
	code = replace(code, L"}", L";};");
	code = replace(code, L"{", L";{;");
	makelowercase(code);
	//std::transform(code.begin(), code.end(), code.begin(), ::tolower);
	while (code.find(L";;") != std::wstring::npos)
	{
		code = replace(code, L";;", L";");
	}
	while (code.find(L"  ") != std::wstring::npos)
	{
		code = replace(code, L"  ", L" ");
	}

	std::vector<std::wstring> lines = split_string(code, L';', skip);
	for (int i = 0; i < lines.size(); i++) 
	{
		//lines can be empty strings
		trim(lines[i]);
	}
	return lines;
}
