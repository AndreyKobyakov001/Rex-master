$INSTANCE = eset;

//Sets the input file and loads.
inputFile = $1;
getta(inputFile);

//Generates relations to track the flow of data.
callbackFuncs = rng(subscribe o call);
controlFlowVars = @isControlFlow . {"\"1\""};

//Set of writes from each variable passed to topic.publish() to the first
//parameter of the callbacks for that topic.
publishWrites = pubVar o pubTarget;

masterRel = varWrite + publishWrites;
masterRel = masterRel+;

//Gets the behaviour alterations.
behAlter = callbackFuncs o masterRel o controlFlowVars;
if $2 == "true" {
	behAlter = inv @label o behAlter o @label;
}
behAlter;
