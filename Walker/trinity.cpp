void ParentWalker::recordVarDecl(const VarDecl *decl) {
    if (langFeats.cVariable){
        // Generates some fields.
        string id = generateID(decl);
        string name = generateName(decl);

       

        // Creates the node.
        RexNode *node = specializedVariableNode(decl, isInMainFile(decl));
        node->addSingleAttribute(TASchemeAttribute::LABEL, name);
        node->addSingleAttribute(TASchemeAttribute::IS_CONTROL_FLOW, "0");
        if (isa<ParmVarDecl>(decl)) {
            node->addSingleAttribute(TASchemeAttribute::IS_PARAM, "1");
            recordNamedDeclLocation(decl, node);
            //node->addSingleAttribute(TASchemeAttribute::FILENAME_DEFINITION, 
            //    generateFileName(decl->getDefinition()));
        } else {
            node->addSingleAttribute(TASchemeAttribute::IS_PARAM, "0");
            recordNamedDeclLocation(decl, node);
        }
        
        addNodeToGraph(node);
        // Get the parent.
        addParentRelationship(decl, id);

        // Store the line number
        varWriteLineNumbers[id] = lineNumber; // Assuming varWriteLineNumbers is a member variable of ParentWalker

        CXXRecordDecl* classType = decl->getCanonicalDecl()->getType().getTypePtr()->getAsCXXRecordDecl();
        if(classType)
        {
            // Record OBJ relation
            addOrUpdateEdge(id, generateID(classType), RexEdge::OBJ);
        }
    }
}
