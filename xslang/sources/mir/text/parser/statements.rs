impl Parser<'_>
{
    fn const_i64_statement(&mut self) -> Statement
    {
        let local = self.const_target();
        let value = self.const_i64_value();
        Statement::ConstI64 {
            local,
            value,
            span: span(),
        }
    }

    fn const_i32_statement(&mut self) -> Statement
    {
        let local = self.const_target();
        let value = self.const_i32_value();
        Statement::ConstI32 {
            local,
            value,
            span: span(),
        }
    }

    fn store_local_statement(&mut self) -> Statement
    {
        let local = self.named_local("target local ");
        let value = self.named_local("value local ");
        Statement::StoreLocal {
            local,
            value,
            span: span(),
        }
    }

    fn load_local_statement(&mut self) -> Statement
    {
        let result = self.named_local("result local ");
        let local = self.named_local("source local ");
        Statement::LoadLocal {
            result,
            local,
            span: span(),
        }
    }

    fn named_local(&mut self, prefix: &str) -> LocalId
    {
        let Some(line) = self.current()
        else
        {
            self.report(format!("missing {prefix}record"));
            return LocalId(0);
        };
        self.index += 1;
        let Some(value) = line.strip_prefix(prefix)
        else
        {
            self.report(format!("expected {prefix}record"));
            return LocalId(0);
        };
        self.local_id(value)
    }

    fn const_i32_value(&mut self) -> i32
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing const.i32 value".to_string());
            return 0;
        };
        self.index += 1;
        let Some(value) = line.strip_prefix("value ")
        else
        {
            self.report("expected const.i32 value".to_string());
            return 0;
        };
        match value.parse()
        {
            Ok(value) => value,
            Err(_) =>
            {
                self.report(format!("invalid const.i32 value '{value}'"));
                0
            }
        }
    }

    fn const_target(&mut self) -> LocalId
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing const.i64 target".to_string());
            return LocalId(0);
        };
        self.index += 1;
        let Some(local) = line.strip_prefix("target local ")
        else
        {
            self.report("expected const.i64 target".to_string());
            return LocalId(0);
        };
        self.local_id(local)
    }

    fn const_i64_value(&mut self) -> i64
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing const.i64 value".to_string());
            return 0;
        };
        self.index += 1;
        let Some(value) = line.strip_prefix("value ")
        else
        {
            self.report("expected const.i64 value".to_string());
            return 0;
        };
        match value.parse()
        {
            Ok(value) => value,
            Err(_) =>
            {
                self.report(format!("invalid const.i64 value '{value}'"));
                0
            }
        }
    }

    fn const_bool_statement(&mut self) -> Statement
    {
        let local = self.const_bool_target();
        let value = self.const_bool_value();
        Statement::ConstBool {
            local,
            value,
            span: span(),
        }
    }

    fn const_bool_target(&mut self) -> LocalId
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing const.bool target".to_string());
            return LocalId(0);
        };
        self.index += 1;
        let Some(local) = line.strip_prefix("target local ")
        else
        {
            self.report("expected const.bool target".to_string());
            return LocalId(0);
        };
        self.local_id(local)
    }

    fn const_bool_value(&mut self) -> bool
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing const.bool value".to_string());
            return false;
        };
        self.index += 1;
        let Some(value) = line.strip_prefix("value ")
        else
        {
            self.report("expected const.bool value".to_string());
            return false;
        };
        match value
        {
            "true" => true,
            "false" => false,
            _ =>
            {
                self.report(format!("invalid const.bool value '{value}'"));
                false
            }
        }
    }

    fn add_i64_statement(&mut self) -> Statement
    {
        let result = self.binary_i64_local("add.i64", "result");
        let left = self.binary_i64_local("add.i64", "left");
        let right = self.binary_i64_local("add.i64", "right");
        Statement::AddI64 {
            result,
            left,
            right,
            span: span(),
        }
    }

    fn sub_i64_statement(&mut self) -> Statement
    {
        let result = self.binary_i64_local("sub.i64", "result");
        let left = self.binary_i64_local("sub.i64", "left");
        let right = self.binary_i64_local("sub.i64", "right");
        Statement::SubI64 {
            result,
            left,
            right,
            span: span(),
        }
    }

    fn mul_i64_statement(&mut self) -> Statement
    {
        let result = self.binary_i64_local("mul.i64", "result");
        let left = self.binary_i64_local("mul.i64", "left");
        let right = self.binary_i64_local("mul.i64", "right");
        Statement::MulI64 {
            result,
            left,
            right,
            span: span(),
        }
    }

    fn eq_i64_statement(&mut self) -> Statement
    {
        let result = self.binary_i64_local("eq.i64", "result");
        let left = self.binary_i64_local("eq.i64", "left");
        let right = self.binary_i64_local("eq.i64", "right");
        Statement::EqI64 {
            result,
            left,
            right,
            span: span(),
        }
    }

    fn not_bool_statement(&mut self) -> Statement
    {
        let result = self.binary_local("not.bool", "result");
        let operand = self.binary_local("not.bool", "operand");
        Statement::NotBool {
            result,
            operand,
            span: span(),
        }
    }

    fn binary_i64_local(&mut self, instruction: &str, field: &str) -> LocalId
    {
        self.binary_local(instruction, field)
    }

    fn binary_local(&mut self, instruction: &str, field: &str) -> LocalId
    {
        let Some(line) = self.current()
        else
        {
            self.report(format!("missing {instruction} {field} local"));
            return LocalId(0);
        };
        self.index += 1;
        let expected = format!("{field} local ");
        let Some(local) = line.strip_prefix(&expected)
        else
        {
            self.report(format!("expected {instruction} {field} local"));
            return LocalId(0);
        };
        self.local_id(local)
    }

    fn call_statement(&mut self) -> Statement
    {
        let function = self.call_function();
        let return_type = self.call_return_type();
        let result = self.call_result();
        let arguments = self.call_arguments();
        Statement::Call {
            result,
            function,
            arguments,
            return_type,
            span: span(),
        }
    }

    fn call_function(&mut self) -> String
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing call function".to_string());
            return String::new();
        };
        self.index += 1;
        match line.strip_prefix("function ")
        {
            Some(function) => function.to_string(),
            None =>
            {
                self.report("expected call function".to_string());
                String::new()
            }
        }
    }

    fn call_return_type(&mut self) -> Type
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing call return type".to_string());
            return Type::VOID;
        };
        self.index += 1;
        let Some(type_name) = line.strip_prefix("returns ")
        else
        {
            self.report("expected call return type".to_string());
            return Type::VOID;
        };
        let value_type = type_from_text(type_name);
        if value_type.is_none()
        {
            self.report(format!("unknown call return type '{type_name}'"));
        }
        value_type.unwrap_or(Type::VOID)
    }

    fn call_result(&mut self) -> Option<LocalId>
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing call result".to_string());
            return None;
        };
        self.index += 1;
        if line == "result discard"
        {
            return None;
        }
        let Some(local) = line.strip_prefix("result local ")
        else
        {
            self.report("expected call result".to_string());
            return None;
        };
        Some(self.local_id(local))
    }

    fn call_arguments(&mut self) -> Vec<LocalId>
    {
        let mut arguments = Vec::new();
        while let Some(line) = self.current()
        {
            let Some(local) = line.strip_prefix("argument local ")
            else
            {
                break;
            };
            self.index += 1;
            arguments.push(self.local_id(local));
        }
        arguments
    }

    fn local_id(&mut self, text: &str) -> LocalId
    {
        match text.parse()
        {
            Ok(id) => LocalId(id),
            Err(_) =>
            {
                self.report(format!("invalid local id '{text}'"));
                LocalId(0)
            }
        }
    }

    fn block_id(&mut self, text: &str) -> BlockId
    {
        match text.parse()
        {
            Ok(id) => BlockId(id),
            Err(_) =>
            {
                self.report(format!("invalid block id '{text}'"));
                BlockId(0)
            }
        }
    }

    fn xmir_version(&mut self)
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing XMIR version header".to_string());
            return;
        };
        let Some(version) = line.strip_prefix(".xmir version ")
        else
        {
            self.report("expected XMIR version header".to_string());
            self.index += 1;
            return;
        };
        match version.parse::<u32>()
        {
            Ok(version) if is_supported_xmir_version(version) =>
            {}
            Ok(version) => self.report(format!(
                "unsupported XMIR version {version}; supported versions are 0 and {SUPPORTED_XMIR_VERSION}"
            )),
            Err(_) => self.report("invalid XMIR version number".to_string()),
        }
        self.index += 1;
    }

    fn next_non_empty(&mut self) -> Option<String>
    {
        while self.current().as_deref() == Some("")
        {
            self.index += 1;
        }
        self.current()
    }

    fn current(&self) -> Option<String>
    {
        self.lines.get(self.index).map(|line| line.trim_start().to_string())
    }

    fn report(&mut self, message: String)
    {
        self.diagnostics.push(XmirParseDiagnostic {
            line: self.index + 1,
            message,
        });
    }
}

const fn span() -> Span
{
    Span::new(0, 0, 0)
}
