$INSTANCE = eset;

//Sets the input file and loads.
inputFile = $1;
getta(inputFile);

//Processes the regular expression system.
if $# >= 2 {
	getcsv($2);
}

//Generates relations to track the flow of data.
callbackFuncs = rng(subscribe o call);
if $# >= 2 {
	for str in dom CSVDATA {
		names = callbackFuncs . @label;
		toRemove = grep(names, str);
		toRemove = toRemove . inv @label;
		callbackFuncs = callbackFuncs - toRemove;
	}
}

controlFlowVars = @isControlFlow . {"\"1\""};
if $# >= 2 {
        for str in dom CSVDATA {
                names = controlFlowVars . @label;
		toRemove = grep(names, str);
		toRemove = toRemove . inv @label;
                controlFlowVars = controlFlowVars - toRemove;
	}
}

//Set of writes from each variable passed to topic.publish() to the first
//parameter of the callbacks for that topic.
publishWrites = pubVar o pubTarget;

masterRel = varWrite + publishWrites + call + write + varInfFunc;
masterRel = masterRel+;

//Gets the behaviour alterations.
behAlter = callbackFuncs o masterRel o controlFlowVars;

//Transforms masterRel
plainMasterRel = inv @label o masterRel o @label;

//Loops through the results.
for item in dom behAlter {
	cbS = {item} . @label;

	//Get the items for this domain.
	vars = {item} . behAlter;
	for var in vars {
		varS = {var} . @label;

		// Print the combination.
		print "####";
		print cbS;
		print varS;
		if $# == 3 {
			for cbSS in cbS { for varSS in varS { showpath(cbSS, varSS, plainMasterRel); } }
		} else {
			showpath(item, var, masterRel);
		}
	}
}
